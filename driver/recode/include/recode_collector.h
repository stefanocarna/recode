#ifndef _RECODE_COLLECTOR_H
#define _RECODE_COLLECTOR_H

#include <linux/sched.h>

#include "pmu_abi.h"

#define BUFFER_MEMORY_SIZE (1024 * 1024 * 256)

struct data_collector_sample {
	pid_t id;
	bool tracked;
	bool k_thread;
	pmc_ctr system_tsc;
	pmc_ctr tsc_cycles;
	pmc_ctr core_cycles;
	pmc_ctr core_cycles_tsc_ref;
	char task_name[TASK_COMM_LEN];
	int tma_level;
	// unsigned ctx_evts;
	struct pmcs_collection pmcs;
	struct tma_collection tma;
} __packed;

struct data_collector {
	uint cpu;
	uint rd_i;
	uint rd_p;
	uint ov_i;
	uint wr_i;
	uint wr_p;
	size_t size;
	u8 raw_memory[];
};

DECLARE_PER_CPU(struct data_collector *, pcpu_data_collector);

extern atomic_t on_dc_samples_flushing;

struct data_collector *init_collector(uint cpu);

void fini_collector(uint cpu);

struct data_collector_sample *get_write_dc_sample(struct data_collector *dc,
						  int pmc_cnt, int tma_cnt);

void put_write_dc_sample(struct data_collector *dc);

struct data_collector_sample *get_read_dc_sample(struct data_collector *dc);

void put_read_dc_sample(struct data_collector *dc);

bool check_read_dc_sample(struct data_collector *dc);

#endif /* _RECODE_COLLECTOR_H */
