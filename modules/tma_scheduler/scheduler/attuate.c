#include "scheduler.h"
#include "system/stats.h"

int start_attuation(void)
{
	int k;
	struct group_entity *group;

	/* Start System CPU stats */
	read_cpu_stats(&sched_conf.cphase_stats.system_cpu_time,
		       &sched_conf.cphase_stats.total_cpu_time);

	/* Start Power stats */
	RR_START();

	for (k = 0; k < this_cphase_cs_part.nr_groups; ++k) {
		group = this_cphase_cs_part.groups[k];

		start_group_stats(group);
		signal_to_group(SIGCONT, this_cphase_cs_part.groups[k]);
	}

	return this_cphase_cs_part.cpu_weight;
}

static bool stop_attuation(void)
{
	int k;
	struct group_entity *group;
	int retire = 0, nr_samples = 0;
	u64 system_cpu_time, total_cpu_time;
	struct tma_profile *profile;

	/* Compute System CPU time */
	read_cpu_stats(&system_cpu_time, &total_cpu_time);
	this_cphase_cs_part.stats.system_cpu_time =
		system_cpu_time - sched_conf.cphase_stats.system_cpu_time;
	this_cphase_cs_part.stats.total_cpu_time =
		total_cpu_time - sched_conf.cphase_stats.total_cpu_time;


	/* Compute Power stats */
	RR_STOP();

	for (k = 0; k < this_cphase_cs_part.nr_groups; ++k) {
		/* Optimize */
		group = this_cphase_cs_part.groups[k];

		stop_group_stats(group);
		signal_to_group(SIGSTOP, group);

		this_cphase_cs_part.stats.cpu_occupancy +=
			(group->utime + group->stime);


		profile = group->data;

		retire += atomic_read(&profile->histotrack_comp[2]);
		nr_samples += atomic_read(&profile->nr_samples);
	}

	/* Compute occupancy % */
	this_cphase_cs_part.stats.cpu_occupancy *= 100;
	this_cphase_cs_part.stats.cpu_occupancy /=
		this_cphase_cs_part.stats.total_cpu_time;

	/* Compute TMA Retire */
	/* TODO Make per-partition */
	this_cphase_cs.retire += retire / nr_samples;

	/* Aggregate to csched stats */
	this_cphase_cs.occupancy += this_cphase_cs_part.stats.cpu_occupancy;
	rapl_read_and_sum_stats(&this_cphase_cs.rapl);

	return !this_cphase_cs_part_has_next();
}

static void attuate_csched(void)
{
	u64 score;
	/* TODO Implement */
	// pr_info("%s - (PARTS %u)\n", __func__, this_cphase_cs.nr_parts);
	pr_info("**\n** ** PART %u ** **\n**\n", sched_conf.cphase_cs_i);
	pr_info("ATTUATION (# %u)\n", this_cphase_cs.nr_parts);

	pr_info("** AVG OCCUPANCY: %u",
		this_cphase_cs.occupancy / this_cphase_cs.nr_parts);
	pr_info("** ENERGY: %llu", this_cphase_cs.rapl.energy_package[0]);
	pr_info("** RETIRE: %u", this_cphase_cs.retire / this_cphase_cs.nr_parts);

	// score = this_cphase_cs.rapl.energy_package[0] << 7;
	// score /= (this_cphase_cs.retire << 7) + 1;

	score = this_cphase_cs.rapl.energy_package[0] * 1000;
	score /= (this_cphase_cs.retire * 1000) + 1;

	pr_info("** BEST SCORE: %llu\n", sched_conf.best_csched_score);
	pr_info("** LAST SCORE: %llu\n", sched_conf.last_csched_score);
	pr_info("** SCORE: %llu\n", score);
	pr_info("**\n** ** ------- ** **\n");

	sched_conf.last_csched_score = score;
}

int attuate_best_sched(void)
{
	/* Return true when the current CS ends */
	if (stop_attuation()) {
		attuate_csched();

		sched_conf.cphase_cs_i = sched_conf.best_csched_i;
		sched_conf.cphase_cs_part_i = 0;

		if (sched_conf.round++ == 10)
			return ERR_TICK;
	} else {
		sched_conf.cphase_cs_part_i++;
	}

	return start_attuation();
}
