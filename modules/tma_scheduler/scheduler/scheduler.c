// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
#include <linux/kernel.h>
#include <linux/hrtimer.h>
#include <linux/jiffies.h>
#include <linux/slab.h>

#include "hooks.h"
#include "plugins/recode_tma.h"

#include "scheduler.h"

struct sched_conf sched_conf;

bool timer_init;
static struct hrtimer timer;
static ktime_t BASE_TICK;

struct group_evaluation *g_evaluations;
int nr_g_evaluations;
int cur_g_evaluation;

struct csched_evaluation *cs_evaluations;
int nr_cs_evaluations;
int cur_cs_evaluation;

static bool init_schedule(void)
{
	uint k;

	if (!nr_groups)
		return false;

	/* Work on current groups */
	sched_conf.nr_groups = nr_groups;
	// sched_conf.groups = kmalloc_array(nr_groups, sizeof(uint), GFP_KERNEL);

	/* Create Group Fingerprints */
	sched_conf.group_fps =
		kmalloc_array(nr_groups, sizeof(struct group_prof), GFP_KERNEL);

	if (!sched_conf.group_fps)
		return false;

	/* Retrieve al Group IDs */
	/* TODO create a dedicated function */
	sched_conf.group_fps[0].group = get_next_group_by_id(0);
	for (k = 1; k < nr_groups; ++k) {
		sched_conf.group_fps[k].group = get_next_group_by_id(
			sched_conf.group_fps[k - 1].group->id);
	}

	/* Used to save data */
	nr_g_evaluations = nr_groups;
	cur_g_evaluation = 0;

	g_evaluations = kmalloc_array(
		nr_groups, sizeof(struct group_evaluation), GFP_KERNEL);

	RR_INIT_ALL();

	sched_conf.state = READY;

	return true;
}

void prepare_evaluation(void)
{
	sched_conf.cur_group = 0;
	sched_conf.state = EVALUATION;

	pr_info("**********************\n");
	pr_info("** START EVALUATION **\n");
	pr_info("**********************\n");

	start_evaluation();
}

#define MIN_CAPACITY 95 /* 95/100 */
#define MAX_CAPACITY (2 * MIN_CAPACITY) /* x/100 */

int prepare_consolidation(void)
{
	int i;
	int mbins;
	int total_occupancy = 0;

	struct bucket *buckets = kmalloc_array(
		sched_conf.nr_groups, sizeof(struct bucket), GFP_KERNEL);

	pr_info("**********************\n");

	for (i = 0; i < sched_conf.nr_groups; ++i) {
		buckets[i].k = sched_conf.group_fps[i].cpu_occupancy;
		buckets[i].payload = sched_conf.group_fps[i].group;

		pr_info("%u]: %llu\n", sched_conf.group_fps[i].group->id,
			sched_conf.group_fps[i].cpu_occupancy);

		total_occupancy += buckets[i].k;
	}

	/* Occupancy should be expressend in x / 100 */
	mbins = total_occupancy / MIN_CAPACITY;
	// mbins = min_bins(buckets, sched_conf.nr_groups, 100);
	pr_info("Required bins: %u\n", mbins);

	sched_conf.nr_available_cs =
		compute_k_partitions_min_max_cap(&sched_conf.available_cs,
						 buckets, sched_conf.nr_groups,
						 MIN_CAPACITY, MAX_CAPACITY);

	pr_info("Available CS: %u\n", sched_conf.nr_available_cs);

	for (int h = 0; h < sched_conf.nr_available_cs; ++h) {
		struct csched *cs = &sched_conf.available_cs[h];

		pr_info("CS [%u] (%u)\n", h, cs->nr_parts);
		for (int s = 0; s < cs->nr_parts; ++s) {
			struct csched_part *cs_p = &cs->parts[s];

			pr_info("\t CSP [%u]: ", s);

			for (int w = 0; w < cs_p->nr_groups; ++w)
				pr_cont(" %u", cs_p->groups[w]->id);
		}
	}

	kfree(buckets);

	/* Used to save data */
	nr_cs_evaluations = sched_conf.nr_available_cs;
	cur_cs_evaluation = 0;

	cs_evaluations =
		kmalloc_array(nr_cs_evaluations,
			      sizeof(struct csched_evaluation), GFP_KERNEL);

	sched_conf.state = CONSOLIDATION;

	pr_info("*************************\n");
	pr_info("** START CONSOLIDATION **\n");
	pr_info("*************************\n");

	return start_consolidation();
};

static void prepare_warmup(void)
{
	signal_to_all_groups(SIGCONT);
	sched_conf.state = WARMUP;
}

int prepare_attuation(void)
{
	sched_conf.cphase_cs_i = sched_conf.best_csched_i;
	sched_conf.cphase_cs_part_i = 0;
	sched_conf.round = 0;
	sched_conf.state = ATTUATION;

	return start_attuation();
}

int schedule_action(void)
{
	switch (sched_conf.state) {
	case UNDEFINED:
		if (!init_schedule())
			break;
		fallthrough;
	case READY:
		pr_info("READY\n");
		prepare_warmup();
		return WARMUP_TICK;
	case WARMUP:
		pr_info("WARMUP\n");
		signal_to_all_groups(SIGSTOP);
		prepare_evaluation();
		return STD_TICK;
	case EVALUATION:
		pr_info("EVALUATION\n");
		if (!evaluate_next_group())
			return prepare_consolidation();
		return STD_TICK;
	case CONSOLIDATION:
		pr_info("CONSOLIDATION\n");
		int time = consolidate_next_partition();

		if (time)
			return time;
		break;	
	// 	return prepare_attuation();
	// case ATTUATION:
	// 	return attuate_best_sched();
	default:
		pr_err("Scheduler wrong state. RESET & STOP\n");
		sched_conf.state = UNDEFINED;
	}

	return ERR_TICK;
}

//Timer callback function
static enum hrtimer_restart hrtimer_hander(struct hrtimer *timer)
{
	int time = schedule_action();

	if (time) {
		pr_info("TICK ADJUSTMENT: %u\n", time);
		ktime_t tick = (time * BASE_TICK) / 100;

		/* If restarting the next wakeup should be moved forward */
		hrtimer_forward_now(
			timer,
			tick); // hrtimer_forward(timer, now, tick_period);
		return HRTIMER_RESTART; //restart timer
	}

	signal_to_all_groups(SIGKILL);
	pr_warn("Schedule doesn't have action to perform. Stop timer\n");
	return HRTIMER_NORESTART;
}

void init_schedule_tick(void)
{
	BASE_TICK = ktime_set(BASE_TICK_SEC, BASE_TICK_NSEC);

	hrtimer_init(&timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL_SOFT);
	timer.function = hrtimer_hander;
	timer_init = true;
}

void enable_scheduler(void)
{
	if (!timer_init)
		init_schedule_tick();

	hrtimer_start(&timer, BASE_TICK, HRTIMER_MODE_REL_SOFT);
}

void disable_scheduler(void)
{
	if (timer_init) {
		hrtimer_try_to_cancel(&timer);
		timer_init = false;
	}
}