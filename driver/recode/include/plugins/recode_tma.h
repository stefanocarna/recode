#ifndef _RECODE_TMA_H
#define _RECODE_TMA_H

#include <linux/atomic.h>

#include "recode_collector.h"

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
	uint level;
	uint cnt;
	atomic64_t nr_samples;
	atomic64_t metrics[10];
};

#define TRACK_DOMAIN 100
#define TRACK_PRECISION 10
#define TRACK_DIV (TRACK_DOMAIN / TRACK_PRECISION)
#define track_index(x) (x / TRACK_DIV)

struct tma_profile {
	atomic_t histotrack[TMA_NR_L3_FORMULAS][TRACK_PRECISION];
	atomic_t nr_samples;
	// struct tma_collection tma;
	refcount_t counter;
};

int recode_tma_init(void);

void recode_tma_fini(void);

struct data_collector_sample *
get_sample_and_compute_tma(struct pmcs_collection *collection, u64 mask,
			   u8 cpu);

size_t get_metrics_size_by_level(uint level);

void compute_tma_metrics_smp(struct pmcs_collection *pmcs_collection,
			     struct tma_collection *tma_collection);

void compute_tma_histotrack_smp(struct pmcs_collection *pmcs_collection,
			     atomic_t (*histotrack)[TRACK_PRECISION], atomic_t *nr_samples);


#endif /* _RECODE_TMA_H */
