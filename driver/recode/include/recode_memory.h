#ifndef _RECODE_MEMORY_H
#define _RECODE_MEMORY_H

#include <linux/sched.h>

#include "pmu_abi.h"

#define BULK_MEMORY_SIZE (1024 * 1024 * 256)

struct memory_bulk {
	struct memory_bulk *next;

	uint used;
	uint free;
	size_t size;

	u8 raw_memory[];
};

struct memory_bulk_manager {
	uint cpu;

	struct memory_bulk *head;
	struct memory_bulk *cur;
};

struct stats_sample {
	int cpu;
	pmc_ctr system_tsc;
	pmc_ctr tsc_cycles;
	int tma_level;
	struct stats_sample *next;
	/* Must be the last - flexible array */ 
	struct tma_collection tma;
} __packed;

DECLARE_PER_CPU(struct memory_bulk_manager *, pcpu_memory_bulk_manager);


struct memory_bulk_manager *init_memory_bulk_menager(uint cpu);

void fini_memory_bulk_menager(uint cpu);

u8 *get_from_memory_bulk_local(size_t amount);

#endif /* _RECODE_MEMORY_H */
