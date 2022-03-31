// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note

#include <linux/slab.h>
#include <linux/sched.h>

#include "pmu.h"
#include "hw_events.h"
#include "pmu_structs.h"
#include "pmu_low.h"
#include "logic/tma.h"

DEFINE_PER_CPU(u8[NR_HW_EVENTS], pcpu_pmcs_index_array);

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

#define SUB_SAFE(a, b) ((a) > (b) ? (a) - (b) : 0)

/* Scale factor */
#define SFACT 100

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

// #define tma_eval_l0_bb(pmcs)
// 	(SUB_SAFE(SFACT,
// 		  tma_eval_l0_fb(pmcs) +
// 			  (SFACT * (EVT_IDX(pmcs, ui_any) +
// 				    (TMA_PIPELINE_WIDTH *
// 				     EVT_IDX(pmcs, im_recovery_cycles)))) /
// 				  (tma_eval_l0_mid_total_slots(pmcs) + 1)))

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
	((SFACT * (EVT_IDX(pmcs, ca_stalls_l3_miss)) /                         \
	  (pmcs[evt_fix_clock_cycles] + 1)) +                                  \
	 tma_eval_l2_mid_br(pmcs) - tma_eval_l2_l2b(pmcs))

#define tma_eval_l2_sb(pmcs)                                                   \
	(EVT_IDX(pmcs, ea_bound_on_stores) / (pmcs[evt_fix_clock_cycles] + 1))

#define computable_tma(tma, mask) ((tma & mask) == tma)

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

enum tma_action { NEXT = 0, STAY = 1, PREV = 2 };

static inline __attribute__((always_inline)) enum tma_action
compute_tms_l0(const struct pmcs_collection *collection)
{
	// uint k;
	// const pmc_ctr *pmcs = collection->pmcs;

	// pr_info("TMA on cpu %u\n", smp_processor_id());
	// pr_info("L0_FB: %llu\n", tma_eval_l0_fb(pmcs));
	// pr_info("L0_BS: %llu\n", tma_eval_l0_bs(pmcs));
	// pr_info("L0_RE: %llu\n", tma_eval_l0_re(pmcs));
	// pr_info("L0_BB: %llu\n", tma_eval_l0_bb(pmcs));

	// pr_info("PMCS: ");
	// for (k = 0; k < collection->cnt; ++k)
	// 	pr_cont(" %llu", collection->pmcs[k]);

	//return tma_eval_l0_re(collection->pmcs) < 300 &&
	//       tma_eval_l0_bb(collection->pmcs) > 30;
	return tma_eval_l0_bb(collection->pmcs) > 20 ? NEXT : STAY;
}

static inline __attribute__((always_inline)) enum tma_action
compute_tms_l1(const struct pmcs_collection *collection)
{
	// pr_debug("CBC: %llu\n", tma_eval_l1_mid_cbc(collection->pmcs));
	// pr_debug("BBC: %llu\n", tma_eval_l1_mid_bbc(collection->pmcs));
	// pr_debug("MBF: %llu\n", tma_eval_l1_mid_mbf(collection->pmcs));
	// pr_debug("L1_MB: %llu\n", tma_eval_l1_mb(collection->pmcs));
	// pr_debug("L1_CB: %llu\n", tma_eval_l1_cb(collection->pmcs));

	// //return false;
	// return tma_eval_l0_bb(collection->pmcs) > 200 &&
	//        tma_eval_l1_mb(collection->pmcs) > 200;
	if (tma_eval_l0_bb(collection->pmcs) > 20 &&
	    tma_eval_l1_mb(collection->pmcs) > 20)
		return NEXT;
	if (tma_eval_l0_bb(collection->pmcs) < 10 &&
	    tma_eval_l1_mb(collection->pmcs) < 10)
		return PREV;
	return STAY;
}

static inline __attribute__((always_inline)) enum tma_action
compute_tms_l2(const struct pmcs_collection *collection)
{
	// pr_debug("stalls_mem_any %llx\n",
	// 	 EVT_IDX(collection->pmcs, ca_stalls_mem_any));
	// pr_debug("stalls_l1d_miss %llx\n",
	// 	 EVT_IDX(collection->pmcs, ca_stalls_l1d_miss));
	// pr_debug("L2_L1B: %llu\n", tma_eval_l2_l1b(collection->pmcs));
	// pr_debug("L2_L2B: %llu\n", tma_eval_l2_l2b(collection->pmcs));
	// pr_debug("L2_L3B: %llu\n", tma_eval_l2_l3b(collection->pmcs));
	// pr_debug("L2_DRAMB: %llu\n", tma_eval_l2_dramb(collection->pmcs));

	// return false;
	return STAY;
}

static inline __attribute__((always_inline)) enum tma_action
compute_tms_l3(const struct pmcs_collection *collection)
{
	// pr_debug("L1_MB: %llu\n", tma_eval_l1_mb(collection->pmcs));
	// pr_debug("L1_CB: %llu\n", tma_eval_l1_cb(collection->pmcs));
	// pr_debug("L2_L1B: %llu\n", tma_eval_l2_l1b(collection->pmcs));
	// pr_debug("L2_L2B: %llu\n", tma_eval_l2_l2b(collection->pmcs));
	// pr_debug("L2_L3B: %llu\n", tma_eval_l2_l3b(collection->pmcs));
	// pr_debug("L2_DRAMB: %llu\n", tma_eval_l2_dramb(collection->pmcs));

	// return tma_eval_l0_bb(collection->pmcs) > 200 &&
	//        tma_eval_l1_mb(collection->pmcs) > 200 &&
	//        (tma_eval_l2_l1b(collection->pmcs) > 100 ||
	// 	tma_eval_l2_l2b(collection->pmcs) > 50 ||
	// 	tma_eval_l2_l3b(collection->pmcs) > 50 ||
	// 	tma_eval_l2_dramb(collection->pmcs) > 100);
	if (tma_eval_l0_bb(collection->pmcs) > 20 &&
	    tma_eval_l1_mb(collection->pmcs) > 20)
		return STAY;
	return PREV;
}

struct tma_level {
	// unsigned threshold;
	uint prev;
	uint next;
	enum tma_action (*compute)(const struct pmcs_collection *collection);
	uint hw_cnt;
	pmc_evt_code *hw_evts;
	struct hw_events *hw_events;
};

struct tma_level gbl_tma_levels[DEFAULT_TMA_MAX_LEVEL];

bool tma_enabled;
EXPORT_SYMBOL(tma_enabled);

DEFINE_PER_CPU(struct tma_collection *, pcpu_tma_collection);
EXPORT_PER_CPU_SYMBOL(pcpu_tma_collection);

int tma_max_level = DEFAULT_TMA_MAX_LEVEL - 1;

/* This function returns the TMA values for the installed level */
void tma_on_pmi_callback_local() //, struct pmus_metadata *pmus_metadata)
{
	// uint k;
	struct pmcs_collection *pmcs_collection;
	struct tma_collection *tma_collection;

	/* pmcs_collection should be correct as long as it accessed here */
	pmcs_collection = this_cpu_read(pcpu_pmus_metadata.pmcs_collection);
	tma_collection = this_cpu_read(pcpu_tma_collection);

	/* TODO Remove */
	if (unlikely(!pmcs_collection)) {
		pr_debug("Got a NULL COLLECTION inside PMI\n");
		return;
	}

	/* TODO Remove */
	if (!this_cpu_read(pcpu_pmus_metadata.hw_events)) {
		pr_debug("Got a NULL hw_events inside PMI\n");
		return;
	}

	tma_collection->level = this_cpu_read(pcpu_pmus_metadata.tma_level);

	compute_tma(pmcs_collection, tma_collection);
}

/* TODO - Caching results may avoid computing indices each level switch */
void update_events_index_local(struct hw_events *events)
{
	u8 idx, tmp;

	pr_info("Update event - mask %llx (cnt: %u)", events->mask,
		events->cnt);

	// TODO Improve code
	pr_debug("Updating index_array on cpu %u\n", smp_processor_id());
	for (idx = 0, tmp = 0; tmp < events->cnt; ++idx) {
		if (events->mask & BIT_ULL(idx)) {
			this_cpu_write(pcpu_pmcs_index_array[idx], tmp + 3);
			tmp++;
		}
	}
}

int tma_init(void)
{
	uint k = 0;
	pmc_evt_code *TMA_HW_EVTS_LEVEL_0;
	pmc_evt_code *TMA_HW_EVTS_LEVEL_1;
	pmc_evt_code *TMA_HW_EVTS_LEVEL_2;
	pmc_evt_code *TMA_HW_EVTS_LEVEL_3;

	for_each_possible_cpu(k) {
		per_cpu(pcpu_tma_collection, k) =
			kmalloc(struct_size(pcpu_tma_collection, metrics,
					    TMA_NR_L3_FORMULAS),
				// sizeof(struct tma_collection) +
				// 	(sizeof(u64) * TMA_NR_L3_FORMULAS),
				GFP_KERNEL);

		if (!per_cpu(pcpu_tma_collection, k))
			goto no_mem;
	}

	TMA_HW_EVTS_LEVEL_0 = kmalloc(sizeof(pmc_evt_code *) * 4, GFP_KERNEL);

	if (!TMA_HW_EVTS_LEVEL_0)
		goto no_mem;

	k = 0;
	TMA_HW_EVTS_LEVEL_0[k++].raw = HW_EVT_COD(iund_core);
	TMA_HW_EVTS_LEVEL_0[k++].raw = HW_EVT_COD(ur_retire_slots);
	TMA_HW_EVTS_LEVEL_0[k++].raw = HW_EVT_COD(ui_any);
	TMA_HW_EVTS_LEVEL_0[k++].raw = HW_EVT_COD(im_recovery_cycles);

	gbl_tma_levels[0].hw_evts = TMA_HW_EVTS_LEVEL_0;
	gbl_tma_levels[0].hw_cnt = k;
	gbl_tma_levels[0].next = 1;
	gbl_tma_levels[0].prev = 0;
	gbl_tma_levels[0].compute = compute_tms_l0;

	TMA_HW_EVTS_LEVEL_1 = kmalloc(sizeof(pmc_evt_code *) * 9, GFP_KERNEL);

	if (!TMA_HW_EVTS_LEVEL_1)
		goto no_tma1;

	k = 0;
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

	TMA_HW_EVTS_LEVEL_2 = kmalloc(sizeof(pmc_evt_code *) * 6, GFP_KERNEL);

	if (!TMA_HW_EVTS_LEVEL_2)
		goto no_tma2;

	k = 0;
	TMA_HW_EVTS_LEVEL_2[k++].raw = HW_EVT_COD(ca_stalls_mem_any);
	TMA_HW_EVTS_LEVEL_2[k++].raw = HW_EVT_COD(ca_stalls_l1d_miss);
	TMA_HW_EVTS_LEVEL_2[k++].raw = HW_EVT_COD(ca_stalls_l2_miss);
	TMA_HW_EVTS_LEVEL_2[k++].raw = HW_EVT_COD(ca_stalls_l3_miss);
	TMA_HW_EVTS_LEVEL_2[k++].raw = HW_EVT_COD(l2_hit);
	TMA_HW_EVTS_LEVEL_2[k++].raw = HW_EVT_COD(l1_pend_miss);

	gbl_tma_levels[2].hw_evts = TMA_HW_EVTS_LEVEL_2;
	gbl_tma_levels[2].hw_cnt = k;
	gbl_tma_levels[2].next = 2;
	gbl_tma_levels[2].prev = 1;
	gbl_tma_levels[2].compute = compute_tms_l2;

	TMA_HW_EVTS_LEVEL_3 = kmalloc(sizeof(pmc_evt_code *) * 14, GFP_KERNEL);

	if (!TMA_HW_EVTS_LEVEL_3)
		goto no_tma3;

	k = 0;
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
	TMA_HW_EVTS_LEVEL_3[k++].raw = HW_EVT_COD(ca_stalls_l2_miss);
	TMA_HW_EVTS_LEVEL_3[k++].raw = HW_EVT_COD(ca_stalls_l3_miss);
	TMA_HW_EVTS_LEVEL_3[k++].raw = HW_EVT_COD(l2_hit);
	TMA_HW_EVTS_LEVEL_3[k++].raw = HW_EVT_COD(l1_pend_miss);

	gbl_tma_levels[3].hw_evts = TMA_HW_EVTS_LEVEL_3;
	gbl_tma_levels[3].hw_cnt = k;
	gbl_tma_levels[3].next = 3;
	gbl_tma_levels[3].prev = 1;
	gbl_tma_levels[3].compute = compute_tms_l3;

	for (k = 0; k < DEFAULT_TMA_MAX_LEVEL; ++k) {
		pr_info("Request event creation (cnt %u)\n",
			gbl_tma_levels[k].hw_cnt);

		gbl_tma_levels[k].hw_events = create_hw_events(
			gbl_tma_levels[k].hw_evts, gbl_tma_levels[k].hw_cnt);

		if (!gbl_tma_levels[k].hw_events)
			goto no_events;

		pr_info("Created event %llx (cnt %u)\n",
			gbl_tma_levels[k].hw_events->mask,
			gbl_tma_levels[k].hw_events->cnt);
	}

	return 0;

no_events:
	for (k--; k >= 0; --k)
		destroy_hw_events(gbl_tma_levels[k].hw_events);
	k = num_possible_cpus();
no_tma3:
	kfree(TMA_HW_EVTS_LEVEL_2);
no_tma2:
	kfree(TMA_HW_EVTS_LEVEL_1);
no_tma1:
	kfree(TMA_HW_EVTS_LEVEL_0);
no_mem:
	while (k--)
		kfree(per_cpu(pcpu_tma_collection, k - 1));

	return -ENOMEM;
}

int enable_tma(int tma_mode)
{
	int k;
	int level;

	if (!tma_mode)
		return -1;

	/* TODO delete level.prev and fox in init func */
	if (tma_mode == 1) {
		pr_warn("*** TMA ENABLED WITH FIXED LEVEL 3 ***\n");
		gbl_tma_levels[3].prev = 3;
		level = 3;
	} else if (tma_mode == 2) {
		pr_warn("*** TMA ENABLED WITH DYNAMIC LEVEL SWITCH ***\n");
		gbl_tma_levels[3].prev = 1;
		level = 0;
	}

	for_each_possible_cpu(k)
		per_cpu(pcpu_pmus_metadata.tma_level, k) = level;

	setup_hw_events_global(gbl_tma_levels[level].hw_events);

	tma_enabled = true;

	return 0;
}

void disable_tma(void)
{
	pmudrv_set_state(false);
	tma_enabled = false;
}

/* This should be changed into enum */
void pmudrv_set_tma(int tma_mode)
{
	pr_info("TMA set to %s\n",
		tma_mode == 0 ? "OFF" : (tma_mode == 1 ? "FIX" : "DYN"));
	if (tma_mode)
		enable_tma(tma_mode);
	else
		disable_tma();
}
EXPORT_SYMBOL(pmudrv_set_tma);

void tma_fini(void)
{
	uint k;

	tma_enabled = false;

	for (k = 0; k < DEFAULT_TMA_MAX_LEVEL; ++k)
		destroy_hw_events(gbl_tma_levels[k].hw_events);

	tma_enabled = true;
}

static __always_inline void switch_tma_level(uint prev_level, uint next_level)
{
	pr_debug("SWITCHING from %u to %u level\n", prev_level, next_level);

	if (prev_level == next_level || next_level > tma_max_level)
		return;

	/* TODO - This must be atomic */
	// TODO Monitor if right
	// this_cpu_write(pcpu_pmus_metadata.tma_level, prev_level);
	this_cpu_write(pcpu_pmus_metadata.tma_level, next_level);
	setup_hw_events_local(gbl_tma_levels[next_level].hw_events);
}

static void reset_tma_level_smp(void *dummy)
{
	int level = this_cpu_read(pcpu_tma_collection)->level;
	switch_tma_level(level, 0);
}

/* This should be changed into enum */
void pmudrv_set_tma_max_level(int tma_level)
{
	disable_tma();

	tma_max_level = tma_level;
	pr_info("TMA max level set to %u\n", tma_max_level);

	on_each_cpu(reset_tma_level_smp, NULL, 1);
}
EXPORT_SYMBOL(pmudrv_set_tma_max_level);

/* TODO Restore */
static void compute_level_switch(int level, struct pmcs_collection *collection)
{
	enum tma_action tma_action;

	tma_action = gbl_tma_levels[level].compute(collection);

	switch (tma_action) {
	case PREV:
		switch_tma_level(level, gbl_tma_levels[level].prev);
		break;
	case NEXT:
		switch_tma_level(level, gbl_tma_levels[level].next);
		break;
	case STAY:
		fallthrough;
	default:
		break;
	}
}

/* tma_collection->level must be prefilled */
void compute_tma(struct pmcs_collection *pmcs_collection,
		 struct tma_collection *tma_collection)
{
#define X_TMA_LEVELS_FORMULAS(name, idx)                                       \
	tma_collection->metrics[idx] = tma_eval_##name(pmcs_collection->pmcs);
	switch (tma_collection->level) {
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
		pr_warn("Unrecognized TMA level %u\n", tma_collection->level);
		return;
	}
#undef X_TMA_LEVELS_FORMULAS
	compute_level_switch(tma_collection->level, pmcs_collection);
}

/* TODO Round - This must be implemented upon basic tma logic */
void compute_tma_histotrack_smp(struct pmcs_collection *pmcs_collection,
				struct tma_collection *tma_collection,
				atomic_t (*histotrack)[TRACK_PRECISION],
				atomic_t(*histotrack_comp),
				atomic_t *nr_samples)
{
#define X_TMA_LEVELS_FORMULAS(name, idx)                                       \
	atomic_inc(&histotrack[idx][track_index(                               \
		tma_eval_##name(pmcs_collection->pmcs))]);                     \
	atomic_add(tma_eval_##name(pmcs_collection->pmcs),                     \
		   &histotrack_comp[idx]);

	switch (tma_collection->level) {
	case 0:
		TMA_L0_FORMULAS
		break;
	case 1:
		TMA_L1_FORMULAS
		break;
	case 2:
		TMA_L2_FORMULAS
		break;
	case 3:
		TMA_L3_FORMULAS
		break;
	default:
		pr_warn("Unrecognized TMA level %u\n", tma_collection->level);
		return;
	}
#undef X_TMA_LEVELS_FORMULAS
	atomic_inc(nr_samples);
	compute_level_switch(tma_collection->level, pmcs_collection);
}
EXPORT_SYMBOL(compute_tma_histotrack_smp);
