#ifndef _RECODE_TMA_H
#define _RECODE_TMA_H

#include <linux/atomic.h>

#include "recode_collector.h"


struct tma_collection {
	uint level;
	uint cnt;
	atomic64_t nr_samples;
	atomic64_t metrics[10];
};

struct tma_profile {
	struct tma_collection tma;
};

int recode_tma_init(void);

void recode_tma_fini(void);

struct data_collector_sample *
get_sample_and_compute_tma(struct pmcs_collection *collection, u64 mask,
			   u8 cpu);

size_t get_metrics_size_by_level(uint level);

void compute_tma_metrics_smp(struct pmcs_collection *pmcs_collection,
			     struct tma_collection *tma_collection);


#endif /* _RECODE_TMA_H */
