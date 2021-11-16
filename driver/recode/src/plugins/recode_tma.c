// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note

#include <linux/slab.h>
#include <linux/sched.h>

#include "pmu_abi.h"
#include "recode.h"
#include "plugins/recode_tma.h"

DEFINE_PER_CPU(u8[20], pcpu_pmcs_index_array);
DEFINE_PER_CPU(u8, pcpu_current_tma_lvl) = 0;

/* TMA masks */

/* Constants */
#define TMA_PIPELINE_WIDTH 4

/* Frontend Bound */
#define TMA_L0_FB (HW_EVT_BIT(iund_core))
/* Bad Speculation */
#define TMA_L0_BS                                                              \
	(HW_EVT_BIT(ur_retire_slots) | HW_EVT_BIT(ui_any) |                    \
	 HW_EVT_BIT(im_recovery_cycles))
/* Retiring */
#define TMA_L0_RE (HW_EVT_BIT(ur_retire_slots))
/* Backend Bound */
#define TMA_L0_BB (TMA_L0_FB | TMA_L0_BS | TMA_L0_RE)

/* Few Uops Executed Threshold */
#define TMA_L1_MID_FUET (HW_EVT_BIT(ea_2_ports_util))

/* Core Bound Cycles */
#define TMA_L1_MID_CBC                                                         \
	(HW_EVT_BIT(ea_exe_bound_0_ports) | HW_EVT_BIT(ea_1_ports_util) |      \
	 TMA_L1_MID_FUET)
/* Backend Bound Cycles */
#define TMA_L1_MID_BBC                                                         \
	(HW_EVT_BIT(ca_stalls_mem_any) | HW_EVT_BIT(ea_bound_on_stores) |      \
	 TMA_L1_MID_CBC)
/* Memory Bound Fraction */
#define TMA_L1_MID_MBF                                                         \
	(HW_EVT_BIT(ca_stalls_mem_any) | HW_EVT_BIT(ea_bound_on_stores) |      \
	 TMA_L1_MID_BBC)
/* Memory Bound */
#define TMA_L1_MB (TMA_L0_BB | TMA_L1_MID_MBF)
/* Core Bound */
#define TMA_L1_CB (TMA_L0_BB | TMA_L1_MB)

/* L2 Bound Ratio */
#define TMA_L2_MID_BR                                                          \
	(HW_EVT_BIT(ca_stalls_l1d_miss) | HW_EVT_BIT(ca_stalls_l2_miss))
/* L1 Bound */
#define TMA_L2_L1B                                                             \
	(HW_EVT_BIT(ca_stalls_mem_any) | HW_EVT_BIT(ca_stalls_l1d_miss))
/* L3 Bound */
#define TMA_L2_L3B                                                             \
	(HW_EVT_BIT(ca_stalls_l2_miss) | HW_EVT_BIT(ca_stalls_l3_miss))
/* L2 Bound */
#define TMA_L2_L2B                                                             \
	(HW_EVT_BIT(l2_hit) | HW_EVT_BIT(l1_pend_miss) | TMA_L2_MID_BR)
/* DRAM Bound */
#define TMA_L2_DRAMB                                                           \
	(HW_EVT_BIT(ca_stalls_l3_miss) | TMA_L2_MID_BR | TMA_L2_L2B)
/* Store Bound */
#define TMA_L2_SB (HW_EVT_BIT(ea_bound_on_stores))

#define TMA_L0 (TMA_L0_BB | TMA_L0_BS | TMA_L0_FB | TMA_L0_RE)
#define TMA_L1 (TMA_L1_CB | TMA_L1_MB)
#define TMA_L2 (TMA_L2_L1B | TMA_L2_L2B | TMA_L2_L3B)
#define TMA_L3 (TMA_L1 | TMA_L2)

/* TMA formulas */

#define SUB_SAFE(a, b) (a > b ? a - b : 0)

/* Scale factor */
#define SFACT 1000

#define EVT_IDX(pmcs, event)                                                   \
	(pmcs[this_cpu_read(pcpu_pmcs_index_array[HW_EVT_IDX(event)])])

#define tma_eval_l0_mid_total_slots(pmcs)                                      \
	(TMA_PIPELINE_WIDTH * pmcs[evt_fix_clock_cycles])

#define tma_eval_l0_fb(pmcs)                                                   \
	((SFACT * EVT_IDX(pmcs, iund_core)) /                                  \
	 (tma_eval_l0_mid_total_slots(pmcs) + 1))

#define tma_eval_l0_bs(pmcs)                                                   \
	((SFACT *                                                              \
	  (EVT_IDX(pmcs, ui_any) - EVT_IDX(pmcs, ur_retire_slots) +            \
	   (TMA_PIPELINE_WIDTH * EVT_IDX(pmcs, im_recovery_cycles)))) /        \
	 (tma_eval_l0_mid_total_slots(pmcs) + 1))

#define tma_eval_l0_re(pmcs)                                                   \
	((SFACT * EVT_IDX(pmcs, ur_retire_slots)) /                            \
	 (tma_eval_l0_mid_total_slots(pmcs) + 1))

#define tma_eval_l0_bb(pmcs)                                                   \
	SUB_SAFE(SFACT, (tma_eval_l0_fb(pmcs) + tma_eval_l0_bs(pmcs) +         \
			 tma_eval_l0_re(pmcs)))

#define tma_eval_l1_mid_fuet(pmcs)                                             \
	(EVT_IDX(pmcs, ea_2_ports_util) *                                      \
	 (EVT_IDX(pmcs, ur_retire_slots) / (pmcs[evt_fix_clock_cycles] + 1)) / \
	 (TMA_PIPELINE_WIDTH + 1))

#define tma_eval_l1_mid_cbc(pmcs)                                              \
	(EVT_IDX(pmcs, ea_exe_bound_0_ports) +                                 \
	 EVT_IDX(pmcs, ea_1_ports_util) + tma_eval_l1_mid_fuet(pmcs))

#define tma_eval_l1_mid_bbc(pmcs)                                              \
	(tma_eval_l1_mid_cbc(pmcs) + EVT_IDX(pmcs, ca_stalls_mem_any) +        \
	 EVT_IDX(pmcs, ea_bound_on_stores))

#define tma_eval_l1_mid_mbf(pmcs)                                              \
	(SFACT *                                                               \
	 (EVT_IDX(pmcs, ca_stalls_mem_any) +                                   \
	  EVT_IDX(pmcs, ea_bound_on_stores)) /                                 \
	 (tma_eval_l1_mid_bbc(pmcs) + 1))

#define tma_eval_l1_mb(pmcs)                                                   \
	((tma_eval_l1_mid_mbf(pmcs) * tma_eval_l0_bb(pmcs)) / SFACT)

#define tma_eval_l1_cb(pmcs)                                                   \
	SUB_SAFE(tma_eval_l0_bb(pmcs), tma_eval_l1_mb(pmcs))

#define tma_eval_l2_mid_br(pmcs)                                               \
	(SFACT *                                                               \
	 SUB_SAFE(EVT_IDX(pmcs, ca_stalls_l1d_miss),                           \
		  EVT_IDX(pmcs, ca_stalls_l2_miss)) /                          \
	 (pmcs[evt_fix_clock_cycles] + 1))

#define tma_eval_l2_l1b(pmcs)                                                  \
	(SFACT *                                                               \
	 (SUB_SAFE(EVT_IDX(pmcs, ca_stalls_mem_any),                           \
		   EVT_IDX(pmcs, ca_stalls_l1d_miss))) /                       \
	 (pmcs[evt_fix_clock_cycles] + 1))

#define tma_eval_l2_l3b(pmcs)                                                  \
	(SFACT *                                                               \
	 (EVT_IDX(pmcs, ca_stalls_l2_miss) -                                   \
	  EVT_IDX(pmcs, ca_stalls_l3_miss)) /                                  \
	 (pmcs[evt_fix_clock_cycles] + 1))

#define tma_eval_l2_l2b(pmcs)                                                  \
	(EVT_IDX(pmcs, l2_hit) * tma_eval_l2_mid_br(pmcs) /                    \
	 (EVT_IDX(pmcs, l2_hit) + EVT_IDX(pmcs, l1_pend_miss) + 1))

#define tma_eval_l2_dramb(pmcs)                                                \
	((EVT_IDX(pmcs, ca_stalls_l3_miss) /                                   \
	  (pmcs[evt_fix_clock_cycles] + 1)) +                                  \
	 tma_eval_l2_mid_br(pmcs) - tma_eval_l2_l2b(pmcs))

#define tma_eval_l2_sb(pmcs)                                                   \
	(EVT_IDX(pmcs, ea_bound_on_stores) / (pmcs[evt_fix_clock_cycles] + 1))

#define computable_tma(tma, mask) ((tma & mask) == tma)

#define TMA_NR_L0_FORMULAS 4
#define TMA_L0_FORMULAS                                                        \
	X_TMA_LEVELS_FORMULAS(l0_bb, 0)                                        \
	X_TMA_LEVELS_FORMULAS(l0_bs, 1)                                        \
	X_TMA_LEVELS_FORMULAS(l0_re, 2)                                        \
	X_TMA_LEVELS_FORMULAS(l0_fb, 3)

#define TMA_NR_L1_FORMULAS 6
#define TMA_L1_FORMULAS                                                        \
	X_TMA_LEVELS_FORMULAS(l0_bb, 0)                                        \
	X_TMA_LEVELS_FORMULAS(l0_bs, 1)                                        \
	X_TMA_LEVELS_FORMULAS(l0_re, 2)                                        \
	X_TMA_LEVELS_FORMULAS(l0_fb, 3)                                        \
	X_TMA_LEVELS_FORMULAS(l1_mb, 4)                                        \
	X_TMA_LEVELS_FORMULAS(l1_cb, 5)

#define TMA_NR_L2_FORMULAS 4
#define TMA_L2_FORMULAS                                                        \
	X_TMA_LEVELS_FORMULAS(l2_l1b, 0)                                       \
	X_TMA_LEVELS_FORMULAS(l2_l2b, 1)                                       \
	X_TMA_LEVELS_FORMULAS(l2_l3b, 2)                                       \
	X_TMA_LEVELS_FORMULAS(l2_dramb, 3)

#define TMA_NR_L3_FORMULAS 10
#define TMA_L3_FORMULAS                                                        \
	X_TMA_LEVELS_FORMULAS(l0_bb, 0)                                        \
	X_TMA_LEVELS_FORMULAS(l0_bs, 1)                                        \
	X_TMA_LEVELS_FORMULAS(l0_re, 2)                                        \
	X_TMA_LEVELS_FORMULAS(l0_fb, 3)                                        \
	X_TMA_LEVELS_FORMULAS(l1_mb, 4)                                        \
	X_TMA_LEVELS_FORMULAS(l1_cb, 5)                                        \
	X_TMA_LEVELS_FORMULAS(l2_l1b, 6)                                       \
	X_TMA_LEVELS_FORMULAS(l2_l2b, 7)                                       \
	X_TMA_LEVELS_FORMULAS(l2_l3b, 8)                                       \
	X_TMA_LEVELS_FORMULAS(l2_dramb, 9)

size_t get_metrics_size_by_level(uint level)
{
	switch (level) {
	case 0:
		return TMA_NR_L0_FORMULAS;
	case 1:
		return TMA_NR_L1_FORMULAS;
	case 2:
		return TMA_NR_L2_FORMULAS;
	case 3:
		return TMA_NR_L3_FORMULAS;
	default:
		return 0;
	}
}

static inline __attribute__((always_inline)) bool
compute_tms_l0(const struct pmcs_collection *collection)
{
	const pmc_ctr *pmcs = collection->pmcs;

	pr_debug("TMA on cpu %u\n", smp_processor_id());
	pr_debug("L0_FB: %llu\n", tma_eval_l0_fb(pmcs));
	pr_debug("L0_BS: %llu\n", tma_eval_l0_bs(pmcs));
	pr_debug("L0_RE: %llu\n", tma_eval_l0_re(pmcs));
	pr_debug("L0_BB: %llu\n", tma_eval_l0_bb(pmcs));

	//return tma_eval_l0_re(collection->pmcs) < 300 &&
	//       tma_eval_l0_bb(collection->pmcs) > 30;
	return tma_eval_l0_bb(pmcs) > 200;
}

static inline __attribute__((always_inline)) bool
compute_tms_l1(const struct pmcs_collection *collection)
{
	pr_debug("CBC: %llu\n", tma_eval_l1_mid_cbc(collection->pmcs));
	pr_debug("BBC: %llu\n", tma_eval_l1_mid_bbc(collection->pmcs));
	pr_debug("MBF: %llu\n", tma_eval_l1_mid_mbf(collection->pmcs));
	pr_debug("L1_MB: %llu\n", tma_eval_l1_mb(collection->pmcs));
	pr_debug("L1_CB: %llu\n", tma_eval_l1_cb(collection->pmcs));

	//return false;
	return tma_eval_l0_bb(collection->pmcs) > 200 &&
	       tma_eval_l1_mb(collection->pmcs) > 200;
}

static inline __attribute__((always_inline)) bool
compute_tms_l2(const struct pmcs_collection *collection)
{
	pr_debug("stalls_mem_any %llx\n",
		 EVT_IDX(collection->pmcs, ca_stalls_mem_any));
	pr_debug("stalls_l1d_miss %llx\n",
		 EVT_IDX(collection->pmcs, ca_stalls_l1d_miss));
	pr_debug("L2_L1B: %llu\n", tma_eval_l2_l1b(collection->pmcs));
	pr_debug("L2_L2B: %llu\n", tma_eval_l2_l2b(collection->pmcs));
	pr_debug("L2_L3B: %llu\n", tma_eval_l2_l3b(collection->pmcs));
	pr_debug("L2_DRAMB: %llu\n", tma_eval_l2_dramb(collection->pmcs));

	return false;
}

static inline __attribute__((always_inline)) bool
compute_tms_l3(const struct pmcs_collection *collection)
{
	pr_debug("L1_MB: %llu\n", tma_eval_l1_mb(collection->pmcs));
	pr_debug("L1_CB: %llu\n", tma_eval_l1_cb(collection->pmcs));
	pr_debug("L2_L1B: %llu\n", tma_eval_l2_l1b(collection->pmcs));
	pr_debug("L2_L2B: %llu\n", tma_eval_l2_l2b(collection->pmcs));
	pr_debug("L2_L3B: %llu\n", tma_eval_l2_l3b(collection->pmcs));
	pr_debug("L2_DRAMB: %llu\n", tma_eval_l2_dramb(collection->pmcs));

	return tma_eval_l0_bb(collection->pmcs) > 200 &&
	       tma_eval_l1_mb(collection->pmcs) > 200 &&
	       (tma_eval_l2_l1b(collection->pmcs) > 100 ||
		tma_eval_l2_l2b(collection->pmcs) > 50 ||
		tma_eval_l2_l3b(collection->pmcs) > 50 ||
		tma_eval_l2_dramb(collection->pmcs) > 100);
}

struct tma_level {
	// unsigned threshold;
	uint prev;
	uint next;
	bool (*compute)(const struct pmcs_collection *collection);
	uint hw_cnt;
	pmc_evt_code *hw_evts;
	struct hw_events *hw_events;
};

#define TMA_MAX_LEVEL 4
struct tma_level gbl_tma_levels[TMA_MAX_LEVEL];

atomic_t pmis = ATOMIC_INIT(0);

void compute_tma_metrics(struct pmcs_collection *pmcs_collection,
			 struct tma_collection *tma_collections)
{
}

void tma_on_pmi_callback(uint cpu, struct pmus_metadata *pmus_metadata)
{
	// uint k;
	struct pmcs_collection *pmcs_collection;
	struct data_collector_sample *dc_sample = NULL;

	atomic_inc(&pmis);

	/* pmcs_collection should be correct as long as it accessed here */
	pmcs_collection = pmus_metadata->pmcs_collection;

	pr_debug("Got PMI on TMA\n");

	if (unlikely(!pmcs_collection)) {
		pr_debug("Got a NULL COLLECTION inside PMI\n");
		return;
	}

	if (!per_cpu(pcpu_pmus_metadata.hw_events, cpu)) {
		pr_debug("Got a NULL hw_events inside PMI\n");
		return;
	}

	u64 mask = per_cpu(pcpu_pmus_metadata.hw_events, cpu)->mask;

	dc_sample = get_sample_and_compute_tma(pmcs_collection, mask, cpu);

	if (unlikely(!dc_sample)) {
		pr_debug("Got a NULL WR SAMPLE inside PMI\n");
		return;
	}

	dc_sample->id = current->pid;
	dc_sample->tracked = query_tracked(current);
	dc_sample->k_thread = !current->mm;

	dc_sample->system_tsc = per_cpu(pcpu_pmus_metadata.last_tsc, cpu);

	dc_sample->tsc_cycles = per_cpu(pcpu_pmus_metadata.sample_tsc, cpu);
	dc_sample->core_cycles = pmcs_fixed(pmcs_collection->pmcs)[1];
	dc_sample->core_cycles_tsc_ref = pmcs_fixed(pmcs_collection->pmcs)[2];
	// dc_sample->ctx_evts = per_cpu(pcpu_pmus_metadata.ctx_evts, cpu);

	get_task_comm(dc_sample->task_name, current);

	/* get_sample_and_compute_tma calls get_write_dc_sample */
	put_write_dc_sample(per_cpu(pcpu_data_collector, cpu));
}

/* TODO - Caching results may avoid computing indices each level switch */
void update_events_index_local(struct hw_events *events)
{
	u8 idx, tmp;

	// TODO Improve code
	pr_debug("Updating index_array on cpu %u\n", smp_processor_id());
	for (idx = 0, tmp = 0; tmp < events->cnt; ++idx) {
		if (events->mask & BIT_ULL(idx)) {
			// pr_debug("Found event on position: %u, val: %u\n", idx,
			// 	 tmp + 3);
			this_cpu_write(pcpu_pmcs_index_array[idx], tmp + 3);
			tmp++;
		}
	}
}

int recode_tma_init(void)
{
	uint k = 0;

	pmc_evt_code *TMA_HW_EVTS_LEVEL_0 =
		kmalloc(sizeof(pmc_evt_code *) * 4, GFP_KERNEL);

	if (!TMA_HW_EVTS_LEVEL_0)
		return -ENOMEM;

	TMA_HW_EVTS_LEVEL_0[k++].raw = HW_EVT_COD(iund_core);
	TMA_HW_EVTS_LEVEL_0[k++].raw = HW_EVT_COD(ur_retire_slots);
	TMA_HW_EVTS_LEVEL_0[k++].raw = HW_EVT_COD(ui_any);
	TMA_HW_EVTS_LEVEL_0[k++].raw = HW_EVT_COD(im_recovery_cycles);

	gbl_tma_levels[0].hw_evts = TMA_HW_EVTS_LEVEL_0;
	gbl_tma_levels[0].hw_cnt = k;
	gbl_tma_levels[0].next = 1;
	gbl_tma_levels[0].prev = 0;
	gbl_tma_levels[0].compute = compute_tms_l0;

	k = 0;
	pmc_evt_code *TMA_HW_EVTS_LEVEL_1 =
		kmalloc(sizeof(pmc_evt_code *) * 9, GFP_KERNEL);

	if (!TMA_HW_EVTS_LEVEL_1)
		return -ENOMEM;

	TMA_HW_EVTS_LEVEL_1[k++].raw = HW_EVT_COD(iund_core);
	TMA_HW_EVTS_LEVEL_1[k++].raw = HW_EVT_COD(ur_retire_slots);
	TMA_HW_EVTS_LEVEL_1[k++].raw = HW_EVT_COD(ui_any);
	TMA_HW_EVTS_LEVEL_1[k++].raw = HW_EVT_COD(im_recovery_cycles);
	TMA_HW_EVTS_LEVEL_1[k++].raw = HW_EVT_COD(ea_exe_bound_0_ports);
	TMA_HW_EVTS_LEVEL_1[k++].raw = HW_EVT_COD(ea_bound_on_stores);
	TMA_HW_EVTS_LEVEL_1[k++].raw = HW_EVT_COD(ea_1_ports_util);
	TMA_HW_EVTS_LEVEL_1[k++].raw = HW_EVT_COD(ea_2_ports_util);
	TMA_HW_EVTS_LEVEL_1[k++].raw = HW_EVT_COD(ca_stalls_mem_any);

	gbl_tma_levels[1].hw_evts = TMA_HW_EVTS_LEVEL_1;
	gbl_tma_levels[1].hw_cnt = k;
	gbl_tma_levels[1].next = 3;
	gbl_tma_levels[1].prev = 0;
	gbl_tma_levels[1].compute = compute_tms_l1;

	k = 0;
	pmc_evt_code *TMA_HW_EVTS_LEVEL_2 =
		kmalloc(sizeof(pmc_evt_code *) * 6, GFP_KERNEL);

	if (!TMA_HW_EVTS_LEVEL_2)
		return -ENOMEM;

	TMA_HW_EVTS_LEVEL_2[k++].raw = HW_EVT_COD(ca_stalls_l1d_miss);
	TMA_HW_EVTS_LEVEL_2[k++].raw = HW_EVT_COD(ca_stalls_mem_any);
	TMA_HW_EVTS_LEVEL_2[k++].raw = HW_EVT_COD(ca_stalls_l2_miss);
	TMA_HW_EVTS_LEVEL_2[k++].raw = HW_EVT_COD(ca_stalls_l3_miss);
	TMA_HW_EVTS_LEVEL_2[k++].raw = HW_EVT_COD(l2_hit);
	TMA_HW_EVTS_LEVEL_2[k++].raw = HW_EVT_COD(l1_pend_miss);

	gbl_tma_levels[2].hw_evts = TMA_HW_EVTS_LEVEL_2;
	gbl_tma_levels[2].hw_cnt = k;
	gbl_tma_levels[2].next = 2;
	gbl_tma_levels[2].prev = 1;
	gbl_tma_levels[2].compute = compute_tms_l2;

	k = 0;
	pmc_evt_code *TMA_HW_EVTS_LEVEL_3 =
		kmalloc(sizeof(pmc_evt_code *) * 10, GFP_KERNEL);

	if (!TMA_HW_EVTS_LEVEL_3)
		return -ENOMEM;

	TMA_HW_EVTS_LEVEL_3[k++].raw = HW_EVT_COD(iund_core);
	TMA_HW_EVTS_LEVEL_3[k++].raw = HW_EVT_COD(ur_retire_slots);
	TMA_HW_EVTS_LEVEL_3[k++].raw = HW_EVT_COD(ui_any);
	TMA_HW_EVTS_LEVEL_3[k++].raw = HW_EVT_COD(im_recovery_cycles);
	TMA_HW_EVTS_LEVEL_3[k++].raw = HW_EVT_COD(ea_exe_bound_0_ports);
	TMA_HW_EVTS_LEVEL_3[k++].raw = HW_EVT_COD(ea_bound_on_stores);
	TMA_HW_EVTS_LEVEL_3[k++].raw = HW_EVT_COD(ea_1_ports_util);
	TMA_HW_EVTS_LEVEL_3[k++].raw = HW_EVT_COD(ea_2_ports_util);
	TMA_HW_EVTS_LEVEL_3[k++].raw = HW_EVT_COD(ca_stalls_mem_any);
	TMA_HW_EVTS_LEVEL_3[k++].raw = HW_EVT_COD(ca_stalls_l1d_miss);
	TMA_HW_EVTS_LEVEL_3[k++].raw = HW_EVT_COD(ca_stalls_mem_any);
	TMA_HW_EVTS_LEVEL_3[k++].raw = HW_EVT_COD(ca_stalls_l2_miss);
	TMA_HW_EVTS_LEVEL_3[k++].raw = HW_EVT_COD(ca_stalls_l3_miss);
	TMA_HW_EVTS_LEVEL_3[k++].raw = HW_EVT_COD(l2_hit);
	TMA_HW_EVTS_LEVEL_3[k++].raw = HW_EVT_COD(l1_pend_miss);

	gbl_tma_levels[3].hw_evts = TMA_HW_EVTS_LEVEL_3;
	gbl_tma_levels[3].hw_cnt = k;
	gbl_tma_levels[3].next = 3;
	gbl_tma_levels[3].prev = 1;
	gbl_tma_levels[3].compute = compute_tms_l3;

	/* Setup recode callbacks */
	/* NOTE - This will be changed into a function */
	// recode_callbacks.on_pmi = on_pmi_callback;
	// recode_callbacks.on_hw_events_change = update_events_index_on_this_cpu;

	for (k = 0; k < TMA_MAX_LEVEL; ++k) {
		gbl_tma_levels[k].hw_events = create_hw_events(
			gbl_tma_levels[k].hw_evts, gbl_tma_levels[k].hw_cnt);

		if (!gbl_tma_levels[k].hw_events)
			goto no_events;
	}

	pr_warn("*** HARDCODED TMA LEVEL 3 ***\n");
	setup_hw_events_global(gbl_tma_levels[3].hw_events);

	return 0;

no_events:
	for (k--; k >= 0; --k)
		destroy_hw_events(gbl_tma_levels[k].hw_events);
	return -ENOMEM;
}

void recode_tma_fini(void)
{
	uint k;

	pr_info("Detected PMIs %u\n", atomic_read(&pmis));

	for (k = 0; k < TMA_MAX_LEVEL; ++k)
		destroy_hw_events(gbl_tma_levels[k].hw_events);
}

static __always_inline void switch_tma_level(uint prev_level, uint next_level)
{
	pr_debug("SWITCHING from %u to %u level\n", prev_level, next_level);

	if (prev_level == next_level)
		return;

	/* TODO - This must be atomic */
	// TODO Monitor if right
	setup_hw_events_local(gbl_tma_levels[next_level].hw_events);
	this_cpu_write(pcpu_current_tma_lvl, next_level);
}

static void evaluate_tma_level(struct pmcs_collection *collection)
{
	uint level = this_cpu_read(pcpu_current_tma_lvl);

	if (gbl_tma_levels[level].compute(collection))
		switch_tma_level(level, gbl_tma_levels[level].next);
	else
		switch_tma_level(level, gbl_tma_levels[level].prev);
}

/* TODO - The mask is now replaced by the level, but this should be changed */
void compute_tma_metrics_smp(struct pmcs_collection *pmcs_collection,
			     struct tma_collection *tma_collection)
{
	uint level = this_cpu_read(pcpu_current_tma_lvl);

#define X_TMA_LEVELS_FORMULAS(name, idx)                                       \
	atomic64_add(tma_eval_##name(pmcs_collection->pmcs),                   \
		     &tma_collection->metrics[idx]);

	switch (level) {
	case 0:
		tma_collection->cnt = TMA_NR_L0_FORMULAS;
		TMA_L0_FORMULAS
		break;
	case 1:
		tma_collection->cnt = TMA_NR_L1_FORMULAS;
		TMA_L1_FORMULAS
		break;
	case 2:
		tma_collection->cnt = TMA_NR_L2_FORMULAS;
		TMA_L2_FORMULAS
		break;
	case 3:
		tma_collection->cnt = TMA_NR_L3_FORMULAS;
		TMA_L3_FORMULAS
		break;
	default:
		pr_warn("Unrecognized TMA level %u\n", level);
		return;
	}
#undef X_TMA_LEVELS_FORMULAS
	atomic64_inc(&tma_collection->nr_samples);

	evaluate_tma_level(pmcs_collection);
}

/* TODO - The mask is now replaced by the level, but this should be changed */
struct data_collector_sample *
get_sample_and_compute_tma(struct pmcs_collection *collection, u64 mask, u8 cpu)
{
	struct data_collector_sample *dc_sample = NULL;
	unsigned level = this_cpu_read(pcpu_current_tma_lvl);

/* This macro is just to save code lines */
#define generate_empty_sample(level)                                           \
	/* Get a sample crafted ad-hoc to fit the current hw_events */         \
	dc_sample = get_write_dc_sample(per_cpu(pcpu_data_collector, cpu),     \
					TMA_NR_L##level##_FORMULAS);           \
	if (unlikely(!dc_sample)) {                                            \
		pr_debug("Got a NULL WR SAMPLE inside PMI\n");                 \
		return NULL;                                                   \
	}                                                                      \
	dc_sample->pmcs.cnt = TMA_NR_L##level##_FORMULAS;                      \
	dc_sample->pmcs.mask = level

#define X_TMA_LEVELS_FORMULAS(name, idx)                                       \
	dc_sample->pmcs.pmcs[idx] = tma_eval_##name(collection->pmcs);
	switch (level) {
	case 0:
		generate_empty_sample(0);
		TMA_L0_FORMULAS
		break;
	case 1:
		generate_empty_sample(1);
		TMA_L1_FORMULAS
		break;
	case 2:
		generate_empty_sample(2);
		TMA_L2_FORMULAS
		break;
	case 3:
		generate_empty_sample(3);
		TMA_L3_FORMULAS
		break;
	default:
		pr_warn("Unrecognized TMA level %u\n", level);
		return NULL;
	}
#undef X_TMA_LEVELS_FORMULAS
	evaluate_tma_level(collection);
	return dc_sample;
}
