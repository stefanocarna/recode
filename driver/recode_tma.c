/* test */
#include <linux/slab.h>

#include "pmu/pmc_events.h"
#include "recode_tma.h"

DEFINE_PER_CPU(u8[HW_EVENTS_NUMBER], pcpu_pmcs_index_array);
DEFINE_PER_CPU(u8, pcpu_current_tma_lvl) = 0;

#define TMA_EVT(name) ((tma_evt_##name.evt).raw)
#define TMA_IDX(name) (tma_evt_##name.idx)
#define TMA_BIT(name) (BIT_ULL(TMA_IDX(name)))

/* TMA events */

/* L0 */
COMPOSE_TMA_EVT(im_recovery_cycles);
COMPOSE_TMA_EVT(ui_any);
COMPOSE_TMA_EVT(iund_core);
COMPOSE_TMA_EVT(ur_retire_slots);

/* L1 */
COMPOSE_TMA_EVT(ca_stalls_mem_any);
COMPOSE_TMA_EVT(ea_bound_on_stores);
COMPOSE_TMA_EVT(ea_exe_bound_0_ports);
COMPOSE_TMA_EVT(ea_1_ports_util);

/* L2 */
COMPOSE_TMA_EVT(ca_stalls_l3_miss);
COMPOSE_TMA_EVT(ca_stalls_l2_miss);
COMPOSE_TMA_EVT(ca_stalls_l1d_miss);
COMPOSE_TMA_EVT(l2_hit);
COMPOSE_TMA_EVT(l1_pend_miss);

/* TMA masks */

/* Constants */
#define TMA_PIPELINE_WIDTH 4

/* Frontend Bound */
#define TMA_L0_FB (TMA_BIT(iund_core))
/* Bad Speculation */
#define TMA_L0_BS                                                              \
	(TMA_BIT(ur_retire_slots) | TMA_BIT(ui_any) |                          \
	 TMA_BIT(im_recovery_cycles))
/* Retiring */
#define TMA_L0_RE (TMA_BIT(ur_retire_slots))
/* Backend Bound */
#define TMA_L0_BB (TMA_L0_FB | TMA_L0_BS | TMA_L0_RE)

/* Few Uops Executed Threshold */
#define TMA_L1_MID_FUET (0)
/* Core Bound Cycles */
#define TMA_L1_MID_CBC                                                         \
	(TMA_BIT(ea_exe_bound_0_ports) | TMA_BIT(ea_1_ports_util) |            \
	 TMA_L1_MID_FUET)
/* Backend Bound Cycles */
#define TMA_L1_MID_BBC                                                         \
	(TMA_BIT(ca_stalls_mem_any) | TMA_BIT(ea_bound_on_stores) |            \
	 TMA_L1_MID_CBC)
/* Memory Bound Fraction */
#define TMA_L1_MID_MBF                                                         \
	(TMA_BIT(ca_stalls_mem_any) | TMA_BIT(ea_bound_on_stores) |            \
	 TMA_L1_MID_BBC)
/* Memory Bound */
#define TMA_L1_MB (TMA_L0_BB | TMA_L1_MID_MBF)
/* Core Bound */
#define TMA_L1_CB (TMA_L0_BB | TMA_L1_MB)

/* L2 Bound Ratio */
#define TMA_L2_MID_BR (TMA_BIT(ca_stalls_l1d_miss) | TMA_BIT(ca_stalls_l2_miss))
/* L1 Bound */
#define TMA_L2_L1B (TMA_BIT(ca_stalls_mem_any) | TMA_BIT(ca_stalls_l1d_miss))
/* L3 Bound */
#define TMA_L2_L3B (TMA_BIT(ca_stalls_l2_miss) | TMA_BIT(ca_stalls_l3_miss))
/* L2 Bound */
#define TMA_L2_L2B (TMA_BIT(l2_hit) | TMA_BIT(l1_pend_miss) | TMA_L2_MID_BR)
/* DRAM Bound */
#define TMA_L2_DRAMB (TMA_BIT(ca_stalls_l3_miss) | TMA_L2_MID_BR | TMA_L2_L2B)
/* Store Bound */
#define TMA_L2_SB (TMA_BIT(ea_bound_on_stores))

#define TMA_L0 (TMA_L0_BB | TMA_L0_BS | TMA_L0_FB | TMA_L0_RE)
#define TMA_L1 (TMA_L1_CB | TMA_L1_MB)
#define TMA_L2 (TMA_L2_L1B | TMA_L2_L2B | TMA_L2_L3B)

/* TMA formulas */

/* Scale factor */
#define SFACT 1000

#define EVT_IDX(pmcs, event)                                                   \
	(pmcs[this_cpu_read(pcpu_pmcs_index_array[TMA_IDX(event)])])

#define tma_eval_l0_mid_total_slots(pmcs)                                      \
	(TMA_PIPELINE_WIDTH * pmcs[evt_fix_clock_cycles])

#define tma_eval_l0_fb(pmcs)                                                   \
	((SFACT * EVT_IDX(pmcs, iund_core)) / (tma_eval_l0_mid_total_slots(pmcs) + 1))

#define tma_eval_l0_bs(pmcs)                                                   \
	((SFACT *                                                              \
	  (EVT_IDX(pmcs, ui_any) - EVT_IDX(pmcs, ur_retire_slots) +            \
	   (TMA_PIPELINE_WIDTH * EVT_IDX(pmcs, im_recovery_cycles)))) /        \
	 (tma_eval_l0_mid_total_slots(pmcs) + 1))

#define tma_eval_l0_re(pmcs)                                                   \
	((SFACT * EVT_IDX(pmcs, ur_retire_slots)) /                            \
	 (tma_eval_l0_mid_total_slots(pmcs) + 1))

#define tma_eval_l0_bb(pmcs)                                                   \
	(SFACT -                                                               \
	 (tma_eval_l0_fb(pmcs) + tma_eval_l0_bs(pmcs) + tma_eval_l0_re(pmcs)))

#define tma_eval_l1_mid_fuet(pmcs) (0)

#define tma_eval_l1_mid_cbc(pmcs)                                              \
	(EVT_IDX(pmcs, ea_exe_bound_0_ports) +                                 \
	 EVT_IDX(pmcs, ea_1_ports_util) + tma_eval_l1_mid_fuet(pmcs))

#define tma_eval_l1_mid_bbc(pmcs)                                              \
	(tma_eval_l1_mid_cbc(pmcs) + EVT_IDX(pmcs, ca_stalls_mem_any) +                 \
	 EVT_IDX(pmcs, ea_bound_on_stores))

#define tma_eval_l1_mid_mbf(pmcs)                                              \
	(SFACT *                                                               \
	 (EVT_IDX(pmcs, ca_stalls_mem_any) +                                   \
	  EVT_IDX(pmcs, ea_bound_on_stores)) /                                 \
	 (tma_eval_l1_mid_bbc(pmcs) + 1))

#define tma_eval_l1_mb(pmcs)                                                   \
	((tma_eval_l1_mid_mbf(pmcs) * tma_eval_l0_bb(pmcs)) / SFACT)

#define tma_eval_l1_cb(pmcs) (tma_eval_l0_bb(pmcs) - tma_eval_l1_mb(pmcs))

#define tma_eval_l2_mid_br(pmcs)                                               \
	(SFACT *                                                               \
	 (EVT_IDX(pmcs, ca_stalls_l1d_miss) -                                  \
	  EVT_IDX(pmcs, ca_stalls_l2_miss)) /                                  \
	 (pmcs[evt_fix_clock_cycles] + 1))

#define tma_eval_l2_l1b(pmcs)                                                  \
	(SFACT *                                                               \
	 (EVT_IDX(pmcs, ca_stalls_mem_any) -                                   \
	  EVT_IDX(pmcs, ca_stalls_l1d_miss)) /                                 \
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

pmc_evt_code TMA_HW_EVTS_LEVEL_0[4] = { { TMA_EVT(iund_core) },
					{ TMA_EVT(ur_retire_slots) },
					{ TMA_EVT(ui_any) },
					{ TMA_EVT(im_recovery_cycles) } };

pmc_evt_code TMA_HW_EVTS_LEVEL_1[8] = { { TMA_EVT(iund_core) },
					{ TMA_EVT(ur_retire_slots) },
					{ TMA_EVT(ui_any) },
					{ TMA_EVT(im_recovery_cycles) },
					{ TMA_EVT(ea_exe_bound_0_ports) },
					{ TMA_EVT(ea_1_ports_util) },
					{ TMA_EVT(ca_stalls_mem_any) },
					{ TMA_EVT(ea_bound_on_stores) } };

pmc_evt_code TMA_HW_EVTS_LEVEL_2[6] = { { TMA_EVT(ca_stalls_mem_any) },
					{ TMA_EVT(ca_stalls_l1d_miss) },
					{ TMA_EVT(ca_stalls_l2_miss) },
					{ TMA_EVT(ca_stalls_l3_miss) },
					{ TMA_EVT(l2_hit) },
					{ TMA_EVT(l1_pend_miss) } };

struct tma_level {
	unsigned threshold;
	struct tma_level *prev;
	struct tma_level *next;
	unsigned hw_cnt;
	pmc_evt_code *hw_evts;
};

#define TMA_MAX_LEVEL 3
struct tma_level gbl_tma_levels[TMA_MAX_LEVEL];

int recode_tma_init(void)
{
	unsigned k;

	gbl_tma_levels[0].hw_cnt = 4;
	gbl_tma_levels[0].hw_evts = TMA_HW_EVTS_LEVEL_0;
	gbl_tma_levels[0].next = &gbl_tma_levels[1];
	gbl_tma_levels[0].prev = &gbl_tma_levels[0];
	gbl_tma_levels[0].threshold = 100;

	gbl_tma_levels[1].hw_cnt = 8;
	gbl_tma_levels[1].hw_evts = TMA_HW_EVTS_LEVEL_1;
	gbl_tma_levels[1].next = &gbl_tma_levels[2];
	gbl_tma_levels[1].prev = &gbl_tma_levels[0];
	gbl_tma_levels[1].threshold = 100;

	gbl_tma_levels[2].hw_cnt = 6;
	gbl_tma_levels[2].hw_evts = TMA_HW_EVTS_LEVEL_2;
	gbl_tma_levels[2].next = &gbl_tma_levels[2];
	gbl_tma_levels[2].prev = &gbl_tma_levels[1];
	gbl_tma_levels[2].threshold = 100;

	for (k = 0; k < TMA_MAX_LEVEL; ++k) {
		setup_hw_events_on_system(gbl_tma_levels[k].hw_evts,
					  gbl_tma_levels[k].hw_cnt);
	}

	pr_warn("EVT %x - idx %u\n", TMA_EVT(im_recovery_cycles),
		TMA_IDX(im_recovery_cycles));
	pr_warn("EVT %x - idx %u\n", TMA_EVT(iund_core), TMA_IDX(iund_core));

	pr_warn("Compute %llu\n", TMA_MASK(2, im_recovery_cycles, iund_core));

	return 0;
}

void check_tma(u32 metrics_size, u64 metrics[], u64 mask)
{
	u32 idx;

	for (idx = 0; idx < metrics_size; ++idx)
		pr_debug("metric %u check result: %s\n", idx,
			 computable_tma(metrics[idx], mask) ? "ok" :
								    "missing event");
}

void print_pmcs_collection(struct pmcs_collection *collection)
{
	u32 idx;

	for (idx = 0; idx < collection->cnt; ++idx)
		pr_debug("%llx ", collection->pmcs[idx]);
}

/* TODO - Caching results may avoid computing indices each level switch */
void update_events_index_on_this_cpu(struct hw_events *events)
{
	u8 idx, tmp;
	pr_debug("Updating index_array on cpu %u\n", smp_processor_id());
	for (idx = 0, tmp = 0; tmp < events->cnt; ++idx) {
		if (events->mask & BIT_ULL(idx)) {
			pr_debug("Found event on position: %u, val: %u\n", idx,
				 tmp + 3);
			this_cpu_write(pcpu_pmcs_index_array[idx], tmp + 3);
			tmp++;
		}
	}
}

void compute_tma(struct pmcs_collection *collection, u64 mask, u8 cpu)
{
	switch (this_cpu_read(pcpu_current_tma_lvl)) {
	case 0:
		if (computable_tma(TMA_L0, mask)) {
			pr_debug("L0_FB: %llu\n",
				 tma_eval_l0_fb(collection->pmcs));
			pr_debug("L0_BS: %llu\n",
				 tma_eval_l0_bs(collection->pmcs));
			pr_debug("L0_RE: %llu\n",
				 tma_eval_l0_re(collection->pmcs));
			pr_debug("L0_BB: %llu\n",
				 tma_eval_l0_bb(collection->pmcs));
			setup_hw_events_on_cpu(gbl_hw_events[1]);
			this_cpu_write(pcpu_current_tma_lvl, 1);
		}
		break;

	case 1:
		if (computable_tma(TMA_L1, mask)) {
			pr_debug("CBC: %llx\n",
				 tma_eval_l1_mid_cbc(collection->pmcs));
			pr_debug("BBC: %llx\n",
				 tma_eval_l1_mid_bbc(collection->pmcs));
			pr_debug("MBF: %llx\n",
				 tma_eval_l1_mid_mbf(collection->pmcs));
			pr_debug("L1_MB: %llx\n",
				 tma_eval_l1_mb(collection->pmcs));
			pr_debug("L1_CB: %llx\n",
				 tma_eval_l1_cb(collection->pmcs));
			setup_hw_events_on_cpu(gbl_hw_events[2]);
			this_cpu_write(pcpu_current_tma_lvl, 2);
		}
		break;

	case 2:
		if (computable_tma(TMA_L2, mask)) {
			pr_debug("stalls_mem_any %llx\n",
				 EVT_IDX(collection->pmcs,
					 ca_stalls_mem_any));
			pr_debug("stalls_l1d_miss %llx\n",
				 EVT_IDX(collection->pmcs,
					 ca_stalls_l1d_miss));
			pr_debug("L2_L1B: %llu\n",
				 tma_eval_l2_l1b(collection->pmcs));
			pr_debug("L2_L2B: %llu\n",
				 tma_eval_l2_l2b(collection->pmcs));
			pr_debug("L2_L3B: %llu\n",
				 tma_eval_l2_l3b(collection->pmcs));
		}
		break;

	default:
		pr_debug("No TMA level to be computed\n");
	}
}

/******************/
