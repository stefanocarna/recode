/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#include "power.h"
#include "recode.h"
#include "plugins/recode_tma.h"

int register_system_hooks(void);

void unregister_system_hooks(void);

void enable_scheduler(void);

void disable_scheduler(void);

void print_tma_metrics(uint id, struct tma_profile *profile);

struct tma_profile *create_process_profile(void);

void destroy_process_profile(struct tma_profile *profile);

void aggregate_tma_profile(struct tma_profile *proc_profile,
			   struct tma_profile *group_profile);

int register_proc_group(void);
int register_proc_csched(void);

struct group_evaluation {
	uint id;
	char gname[TASK_COMM_LEN];
	uint *groups_id;
	uint nr_groups;
	struct tma_profile profile;
	/* CPU Stats */
	u64 cpu_time;
	u64 total_time;
	int nr_active_tasks;
	struct rapl_stats rapl;
};

extern struct group_evaluation *g_evaluations;
extern int nr_g_evaluations;
extern int cur_g_evaluation;

struct csched_part_evaluation {
	uint *group_ids;
	uint nr_groups;
};

struct csched_evaluation {
	u64 score;
	u64 occupancy;
	u64 energy;
	u64 retire;

	int nr_parts;
	struct csched_part_evaluation *parts;
};

extern struct csched_evaluation *cs_evaluations;
extern int nr_cs_evaluations;
extern int cur_cs_evaluation;


struct group_step {
	uint id;
	char gname[TASK_COMM_LEN];
	uint *groups_id;
	uint nr_groups;
	struct tma_profile profile;
	/* CPU Stats */
	u64 cpu_time;
	u64 total_time;
	int nr_active_tasks;
	struct rapl_stats rapl;
};

// extern struct group_step *gsteps;
// extern uint nr_gsteps;

void read_cpu_stats(u64 *used, u64 *total);


/* Scheduler */
struct sched_stats {
	/* CPU info */
	u64 used_cpu_time;
	u64 system_cpu_time;
	u64 total_cpu_time;

	u64 cpu_occupancy;

	/* Power info */
	struct rapl_stats rapl;
};

struct csched_part {
	struct group_entity **groups;
	uint nr_groups;

	int cpu_weight; /* Partition weight used to adjust the TICK */

	struct sched_stats stats;
	struct tma_profile profile;
};

struct csched {
	struct csched_part *parts;
	int nr_parts;

	u64 score;
	int occupancy; /* Deprecated */
	int efficiency;
	int retire;
	struct rapl_stats rapl;
};


/* Algo */
struct bucket {
	int k;
	void *payload;
};

int min_bins(struct bucket *buckets, size_t size, int cap);

size_t compute_k_partitions_min_max_cap(struct csched **av_cs_p,
				    struct bucket *buckets, size_t size,
				    int min_cap, int max_cap);

size_t compute_k_partitions_max_cap(struct csched **av_cs,
				struct bucket *buckets, size_t size, int k,
				int cap);

