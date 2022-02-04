#ifndef _RECODE_TMA_H
#define _RECODE_TMA_H

#include <linux/atomic.h>
#include <linux/refcount.h>

#define TMA_L0_FORMULAS                                                        \
	X_TMA_LEVELS_FORMULAS(l0_bb, 0)                                        \
	X_TMA_LEVELS_FORMULAS(l0_bs, 1)                                        \
	X_TMA_LEVELS_FORMULAS(l0_re, 2)                                        \
	X_TMA_LEVELS_FORMULAS(l0_fb, 3)
#define TMA_NR_L0_FORMULAS 4

#define TMA_L1_FORMULAS                                                        \
	X_TMA_LEVELS_FORMULAS(l0_bb, 0)                                        \
	X_TMA_LEVELS_FORMULAS(l0_bs, 1)                                        \
	X_TMA_LEVELS_FORMULAS(l0_re, 2)                                        \
	X_TMA_LEVELS_FORMULAS(l0_fb, 3)                                        \
	X_TMA_LEVELS_FORMULAS(l1_mb, 4)                                        \
	X_TMA_LEVELS_FORMULAS(l1_cb, 5)
#define TMA_NR_L1_FORMULAS 6

#define TMA_L2_FORMULAS                                                        \
	X_TMA_LEVELS_FORMULAS(l2_l1b, 0)                                       \
	X_TMA_LEVELS_FORMULAS(l2_l2b, 1)                                       \
	X_TMA_LEVELS_FORMULAS(l2_l3b, 2)                                       \
	X_TMA_LEVELS_FORMULAS(l2_dramb, 3)
#define TMA_NR_L2_FORMULAS 4

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
#define TMA_NR_L3_FORMULAS 10

struct tma_collection {
	int level;
	int cnt;
	u64 metrics[];
};

#define TRACK_DOMAIN 100
#define TRACK_PRECISION 100
#define TRACK_DIV (TRACK_DOMAIN / TRACK_PRECISION)
#define track_index(x) (x / TRACK_DIV)

struct tma_profile {
	atomic_t histotrack[TMA_NR_L3_FORMULAS][TRACK_PRECISION];
	atomic_t histotrack_comp[TMA_NR_L3_FORMULAS];
	atomic_t nr_samples;
	// struct tma_collection tma;
	refcount_t counter;
	/* CPU Stats */
	u64 time;
};

DECLARE_PER_CPU(struct tma_collection *, pcpu_tma_collection);

extern bool tma_enabled;

int tma_init(void);

void tma_fini(void);

void update_events_index_local(struct hw_events *events);

void tma_on_pmi_callback_local(void);

void pmudrv_set_tma(bool tma);

void compute_tma(struct pmcs_collection *pmu_collection,
		 struct tma_collection *tma_collection);

void compute_tma_metrics_smp(struct pmcs_collection *pmcs_collection,
			     struct tma_collection *tma_collection);

void compute_tma_histotrack_smp(struct pmcs_collection *pmcs_collection,
				atomic_t (*histotrack)[TRACK_PRECISION],
				atomic_t(*histotrack_comp),
				atomic_t *nr_samples);

#endif /* _RECODE_TMA_H */
