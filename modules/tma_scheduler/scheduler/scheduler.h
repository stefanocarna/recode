

#include "../tma_scheduler.h"

#define this_cphase_cs (sched_conf.available_cs[sched_conf.cphase_cs_i])
#define this_cphase_cs_part                                                    \
	(sched_conf.available_cs[sched_conf.cphase_cs_i]                       \
		 .parts[sched_conf.cphase_cs_part_i])
#define this_cphase_complete()                                                 \
	(sched_conf.cphase_cs_i >= sched_conf.nr_available_cs)
#define this_cphase_cs_complete()                                              \
	(sched_conf.cphase_cs_part_i >= this_cphase_cs.nr_parts)
#define this_cphase_cs_part_has_next()                                         \
	(sched_conf.cphase_cs_part_i < this_cphase_cs.nr_parts - 1)

#define ERR_TICK	0
#define STD_TICK	100
#define WARMUP_TICK	(5 * STD_TICK)
#define MAX_ROUND	10

#define BASE_TICK_SEC	1
#define BASE_TICK_NSEC	20000000

enum sched_state {
	UNDEFINED = 0,
	WARMUP,
	READY,
	EVALUATION,
	CONSOLIDATION,
	ATTUATION,
};

struct sched_conf {
	enum sched_state state;

	/* Filled during the EVALUATION phase */
	struct group_prof *group_fps;
	int cur_group;
	int nr_groups;

	/* Filled during the ATTUATION phase */
	struct csched *available_cs;
	int nr_available_cs;

	/* Consolidation phase */
	int cphase_cs_i;
	int cphase_cs_part_i;
	struct sched_stats cphase_stats;

	/* Attuation phase */
	int round;
	int best_csched_i;
	u64 best_csched_score;
	u64 last_csched_score;
};

struct group_prof {
	struct group_entity *group;

	/* CPU stats */
	u64 cpu_used_time;
	u64 cpu_total_time;

	u64 cpu_occupancy;

	/* Other stats */
};

extern struct sched_conf sched_conf; 

void start_evaluation(void);
bool evaluate_next_group(void);

int start_consolidation(void);
int consolidate_next_partition(void);

int start_attuation(void);
int attuate_best_sched(void);