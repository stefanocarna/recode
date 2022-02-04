#include "scheduler.h"
#include <linux/slab.h>

int start_consolidation(void)
{
	int k;
	struct group_entity *group;

	/* Start System CPU stats */
	read_cpu_stats(&sched_conf.cphase_stats.system_cpu_time,
		       &sched_conf.cphase_stats.total_cpu_time);

	// pr_info("** START CPU USAGE:  %llu / %llu\n",
	// 	sched_conf.cphase_stats.system_cpu_time,
	// 	sched_conf.cphase_stats.total_cpu_time);

	/* Start Power stats */
	RR_START();

	pr_info("SCHED PART (%u):(%u)\n", sched_conf.cphase_cs_part_i,
		sched_conf.cphase_cs_i);

	for (k = 0; k < this_cphase_cs_part.nr_groups; ++k) {
		group = this_cphase_cs_part.groups[k];

		start_group_stats(group);
		signal_to_group(SIGCONT, this_cphase_cs_part.groups[k]);
	}

	pr_info("WEIGHT CONS %u\n", this_cphase_cs_part.cpu_weight);

	return this_cphase_cs_part.cpu_weight;
}

static bool stop_consolidation(void)
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

	pr_info("** CPU USAGE:  %llu / %llu\n",
		this_cphase_cs_part.stats.system_cpu_time,
		this_cphase_cs_part.stats.total_cpu_time);

	/* Compute Power stats */
	RR_STOP();

	for (k = 0; k < this_cphase_cs_part.nr_groups; ++k) {
		/* Optimize */
		group = this_cphase_cs_part.groups[k];

		stop_group_stats(group);
		signal_to_group(SIGSTOP, group);

		this_cphase_cs_part.stats.cpu_occupancy +=
			(group->utime + group->stime);

		// pr_info("GROUP CPU T: %llu\n", (group->utime + group->stime));

		profile = group->data;

		retire += atomic_read(&profile->histotrack_comp[2]);
		nr_samples += atomic_read(&profile->nr_samples);
	}

	/* Compute occupancy % */
	this_cphase_cs_part.stats.cpu_occupancy *= 100;
	this_cphase_cs_part.stats.cpu_occupancy /=
		(this_cphase_cs_part.stats.total_cpu_time + 1);

	pr_info("%u] Occupancy: %llu\n", sched_conf.cphase_cs_part_i,
		this_cphase_cs_part.stats.cpu_occupancy);

	/* Compute TMA Retire */
	/* TODO Make per-partition */
	if (nr_samples != 0)
		this_cphase_cs.retire += retire / nr_samples;

	// pr_info("CPU OCC (%u): %llu\n", sched_conf.cphase_cs_part_i,
	// 	this_cphase_cs_part.stats.cpu_occupancy);

	/* Aggregate to csched stats */
	this_cphase_cs.occupancy += this_cphase_cs_part.stats.cpu_occupancy;
	rapl_read_and_sum_stats(&this_cphase_cs.rapl);

	// pr_info("** G %u/%u ** STOP consolidation\n", sched_conf.cphase_cs_i,
	// 	sched_conf.nr_available_cs);

	return !this_cphase_cs_part_has_next();
}

/* @ each csched */
void save_consolidation(void)
{
	int i, j;
	struct csched_part *cs_part;
	struct csched_evaluation *csched_ev;
	struct csched_part_evaluation *cs_part_ev;

	csched_ev = &cs_evaluations[cur_cs_evaluation];

	csched_ev->nr_parts = this_cphase_cs.nr_parts;
	csched_ev->parts = kmalloc_array(csched_ev->nr_parts,
					 sizeof(struct csched_part_evaluation),
					 GFP_KERNEL);

	for (i = 0; i < this_cphase_cs.nr_parts; ++i) {
		cs_part = &this_cphase_cs.parts[i];
		cs_part_ev = &csched_ev->parts[i];
		cs_part_ev->nr_groups = cs_part->nr_groups;

		cs_part_ev->group_ids = kmalloc_array(cs_part_ev->nr_groups,
						      sizeof(uint), GFP_KERNEL);

		for (j = 0; j < cs_part_ev->nr_groups; ++j)
			cs_part_ev->group_ids[j] = cs_part->groups[j]->id;
	}

	// score = this_cphase_cs.rapl.energy_package[0] * this_cphase_cs.retire * this_cphase_cs.occuoancy;
	// score /= (this_cphase_cs.retire * 1000) + 1;

	csched_ev->occupancy =
		this_cphase_cs.occupancy / this_cphase_cs.nr_parts;
	csched_ev->retire = this_cphase_cs.retire / this_cphase_cs.nr_parts;
	csched_ev->energy = this_cphase_cs.rapl.energy_package[0];
	csched_ev->score = (csched_ev->energy * csched_ev->retire * csched_ev->occupancy) / 100;

	cur_cs_evaluation++;
}

static void consolidate_csched(void)
{
	u64 score;
	/* TODO Implement */
	// pr_info("%s - (PARTS %u)\n", __func__, this_cphase_cs.nr_parts);
	pr_info("**\n** ** PART %u ** **\n**\n", sched_conf.cphase_cs_i);
	pr_info("CONSOLIDATION (# %u)\n", this_cphase_cs.nr_parts);

	pr_info("** AVG OCCUPANCY: %u",
		this_cphase_cs.occupancy / this_cphase_cs.nr_parts);
	pr_info("** ENERGY: %llu", this_cphase_cs.rapl.energy_package[0]);
	pr_info("** RETIRE: %u",
		this_cphase_cs.retire / this_cphase_cs.nr_parts);

	// score = this_cphase_cs.rapl.energy_package[0] << 7;
	// score /= (this_cphase_cs.retire << 7) + 1;

	score = this_cphase_cs.rapl.energy_package[0] * 1000;
	score /= (this_cphase_cs.retire * 1000) + 1;

	pr_info("** SCORE: %llu\n", score);
	pr_info("**\n** ** ------- ** **\n");

	if (score > sched_conf.best_csched_score) {
		sched_conf.best_csched_score = score;
		sched_conf.best_csched_i = sched_conf.cphase_cs_i;
	}
}

int consolidate_next_partition(void)
{
	/* Check there are cscheds left */
	if (this_cphase_complete())
		return ERR_TICK;

	/* Return true when the current CS ends */
	if (stop_consolidation()) {
		consolidate_csched();
		save_consolidation();

		do {
			sched_conf.cphase_cs_i++;
			sched_conf.cphase_cs_part_i = 0;
		} while (this_cphase_cs_complete());

		// pr_info("AV CS %u - CS_i %u\n", sched_conf.nr_available_cs,
		// 	sched_conf.cphase_cs_i);

		if (this_cphase_complete())
			return ERR_TICK;
	} else {
		sched_conf.cphase_cs_part_i++;
	}

	return start_consolidation();
}
