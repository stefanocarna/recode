#include "scheduler.h"

void start_evaluation(void)
{
	struct group_prof *group_fp;

	group_fp = &sched_conf.group_fps[sched_conf.cur_group];

	/* Start System CPU time */
	read_cpu_stats(&group_fp->cpu_used_time, &group_fp->cpu_total_time);

	pr_info(">> G %p ** %u\n", group_fp->group, group_fp->group->id);
	pr_info(">> G %s ** START\n", group_fp->group->name);
	/* Start Power stats */
	RR_START();

	/* Start Group Stats */
	start_group_stats(group_fp->group);
	signal_to_group(SIGCONT, group_fp->group);
}

void stop_evaluation(void)
{
	u64 used_time, total_time;
	struct group_prof *group_fp;

	group_fp = &sched_conf.group_fps[sched_conf.cur_group];

	/* Compute System CPU time */
	read_cpu_stats(&used_time, &total_time);
	group_fp->cpu_used_time = used_time - group_fp->cpu_used_time;
	group_fp->cpu_total_time = total_time - group_fp->cpu_total_time;

	pr_info("- CPU USAGE:  %llu / %llu\n", group_fp->cpu_used_time,
		group_fp->cpu_total_time);

	/* Compute Power stats */
	RR_STOP();

	/* Compute Group Stats */
	stop_group_stats(group_fp->group);

	group_fp->cpu_occupancy =
		(100 * (group_fp->group->utime + group_fp->group->stime)) /
		(group_fp->cpu_total_time + 1);

	signal_to_group(SIGSTOP, group_fp->group);

	pr_info("<< G %p ** %u\n", group_fp->group, group_fp->group->id);
	pr_info("<< G %s ** STOP\n", group_fp->group->name);
}

void save_evaluation(void)
{
	struct group_entity *group;
	struct group_evaluation *group_ev;
	struct group_prof *group_fp;

	group_fp = &sched_conf.group_fps[sched_conf.cur_group];
	group_ev = &g_evaluations[cur_g_evaluation];
	group = group_fp->group;

	/* Copy Group info */
	group_ev->id = group->id;
	memcpy(group_ev->gname, group->name, TASK_COMM_LEN);
	group_ev->nr_active_tasks = group->nr_active_tasks;

	/* Copy TMA profile */
	memcpy(&group_ev->profile, group->data, sizeof(struct tma_profile));

	/* Copy CPU stats */
	group_ev->cpu_time = group_fp->cpu_used_time;
	group_ev->total_time = group_fp->cpu_total_time;
	group_ev->profile.time = group->utime + group->stime;

	/* Copy Power stats */
	rapl_read_stats(&group_ev->rapl);

	// pr_info("** G %s ** STORE evaluation\n", group->name);

	/* Reset the group profile */
	// memset(profile, 0, sizeof(struct tma_profile));
	// group->utime = 0;
	// group->stime = 0;
	// cur_gsteps++;

	cur_g_evaluation++;

	// RR_PRINT(RAPL_PRINT_ALL);
}

bool evaluate_next_group(void)
{
	if (sched_conf.cur_group == sched_conf.nr_groups)
		return false;

	stop_evaluation();

	save_evaluation();

	sched_conf.cur_group++;
	if (sched_conf.cur_group == sched_conf.nr_groups)
		return false;

	start_evaluation();

	return true;
}