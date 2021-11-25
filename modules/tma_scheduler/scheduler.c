// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note

#include <linux/kernel.h>
#include <linux/hrtimer.h>
#include <linux/jiffies.h>
#include <linux/slab.h>

#include "hooks.h"
#include "recode.h"
#include "plugins/recode_tma.h"

#include "tma_scheduler.h"

struct group_step *gsteps;
uint nr_gsteps;
uint cur_gsteps;

static bool init;
static ktime_t kt_interval;
static struct hrtimer timer;

struct session_step {
	uint r;
	uint **groups;
	size_t nr_groups;
};

struct sched_conf {
	bool init;
	uint *groups;
	uint nr_groups;
	struct session_step *sessions;
	uint cur_step;
	uint cur_session;
};

static struct sched_conf sched_conf;

void fillSet(int data[], int start, int end, int index, int r)
{
	if (index == r) {
		uint *groups = kmalloc_array(r, sizeof(uint), GFP_KERNEL);

		sched_conf.sessions[r - 1].groups[sched_conf.cur_step] = groups;
		sched_conf.cur_step++;

		pr_info("R: %u\n", r);
		for (int j = 0; j < r; j++) {
			groups[j] = data[j];
			pr_info("%u ", data[j]);
		}
		pr_info("\n");

		return;
	}

	for (int i = start; i <= end && end - i + 1 >= r - index; i++) {
		data[index] = sched_conf.groups[i];
		fillSet(data, i + 1, end, index + 1, r);
	}
}

static void signal_to_group_set(uint signal, uint *groups, size_t nr)
{
	uint k;

	for (k = 0; k < nr; ++k) {
		pr_info("Signal %s to %u\n",
			signal == SIGCONT ? "CONT" : "STOP", groups[k]);
		signal_to_group(signal, groups[k]);
	}
}

void stop_previous_step(void)
{
	uint session, step;

	if (sched_conf.cur_step == 0) {
		session = sched_conf.cur_session - 1;
		step = sched_conf.sessions[session].nr_groups - 1;
	} else {
		session = sched_conf.cur_session;
		step = sched_conf.cur_step - 1;
	}

	signal_to_group_set(SIGSTOP, sched_conf.sessions[session].groups[step],
			    sched_conf.sessions[session].r);
}

void save_profile_previous_step(void)
{
	uint *groups;
	uint k, session, step;

	if (sched_conf.cur_step == 0) {
		session = sched_conf.cur_session - 1;
		step = sched_conf.sessions[session].nr_groups - 1;
	} else {
		session = sched_conf.cur_session;
		step = sched_conf.cur_step - 1;
	}

	groups = sched_conf.sessions[session].groups[step];

	for (k = 0; k < sched_conf.sessions[session].r; ++k) {
		gsteps[cur_gsteps].id = groups[k];
		gsteps[cur_gsteps].groups_id =
			kmalloc_array(sched_conf.sessions[session].r,
				      sizeof(uint), GFP_KERNEL);
		memcpy(gsteps[cur_gsteps].groups_id, groups,
		       array_size(sched_conf.sessions[session].r,
				  sizeof(uint)));
		gsteps[cur_gsteps].nr_groups = sched_conf.sessions[session].r;
		memcpy(&gsteps[cur_gsteps].profile,
		       get_group_by_id(groups[k])->data,
		       sizeof(struct tma_profile));
		cur_gsteps++;
	}
}

size_t nr_sets(uint n, uint r)
{
	uint k, nrf = 1, rf = 1, nf = 1;

	for (k = 2; k <= n; ++k)
		nf *= k;

	for (k = 2; k <= r; ++k)
		rf *= k;

	for (k = 2; k <= (n - r); ++k)
		nrf *= k;

	pr_info("nf: %u, rf: %u, nrf: %u - n: %u - r: %u\n", nf, rf, nrf, n, r);
	pr_info("nr_sets: %u\n", nf / (rf * nrf));

	return nf / (rf * nrf);
}

static bool init_schedule(void)
{
	uint k;
	uint *tmp_set;

	nr_gsteps = 0;
	cur_gsteps = 0;

	if (!nr_groups)
		return false;

	tmp_set = kmalloc_array(nr_groups, sizeof(uint), GFP_KERNEL);

	sched_conf.groups = kmalloc_array(nr_groups, sizeof(uint), GFP_KERNEL);
	sched_conf.groups[0] = get_next_group_by_id(0)->id;
	pr_info("g%u: %u\n", 0, sched_conf.groups[0]);
	for (k = 1; k < nr_groups; ++k) {
		sched_conf.groups[k] =
			get_next_group_by_id(sched_conf.groups[k - 1])->id;
		pr_info("g%u: %u\n", k, sched_conf.groups[k]);
	}

	sched_conf.nr_groups = nr_groups;
	sched_conf.sessions = kmalloc_array(
		nr_groups, sizeof(struct session_step), GFP_KERNEL);

	for (k = 0; k < nr_groups; ++k) {
		sched_conf.sessions[k].nr_groups = nr_sets(nr_groups, k + 1);

		nr_gsteps += sched_conf.sessions[k].nr_groups * (k + 1);

		sched_conf.sessions[k].groups =
			kmalloc_array(sched_conf.sessions[k].nr_groups,
				      sizeof(uint *), GFP_KERNEL);
		sched_conf.sessions[k].r = k + 1;
		fillSet(tmp_set, 0, nr_groups - 1, 0, k + 1);
		sched_conf.cur_step = 0;
	}

	gsteps =
		kmalloc_array(nr_gsteps, sizeof(struct group_step), GFP_KERNEL);

	kfree(tmp_set);
	sched_conf.init = true;

	return true;
}

bool schedule_next_group(void)
{
	/* Init the session if required */
	if (!sched_conf.init)
		if (!init_schedule())
			return false;

	/* First iteration */
	if (!sched_conf.cur_step && !sched_conf.cur_session)
		goto skip_stop;

	stop_previous_step();

	save_profile_previous_step();

	/* End of the session */
	if (sched_conf.cur_session == sched_conf.nr_groups) {
		signal_to_all_groups(SIGKILL);
		init = false;
		return false;
	}

skip_stop:
	pr_info("Sess: %u - Step: %u - r: %u\n", sched_conf.cur_session,
		sched_conf.cur_step,
		sched_conf.sessions[sched_conf.cur_session].r);
	signal_to_group_set(SIGCONT,
			    sched_conf.sessions[sched_conf.cur_session]
				    .groups[sched_conf.cur_step],
			    sched_conf.sessions[sched_conf.cur_session].r);

	/* Move to next session */
	if (sched_conf.cur_step ==
	    sched_conf.sessions[sched_conf.cur_session].nr_groups - 1) {
		/* Next session */
		sched_conf.cur_session++;
		sched_conf.cur_step = 0;
	} else {
		/* Next step */
		sched_conf.cur_step++;
	}

	pr_info("END Sess: %u - Step: %u - r: %u\n", sched_conf.cur_session,
		sched_conf.cur_step,
		sched_conf.sessions[sched_conf.cur_session].r);

	pr_info("GUARD\n");
	return true;
}

// static bool schedule_next_group(void)
// {
// 	struct group_entity *old_group = get_group_by_id(group_id);
// 	struct group_entity *group = get_next_group_by_id(group_id);

// 	if (!group) {
// 		pr_warn("Cannot schedule without groups\n");
// 		return false;
// 	}

// 	group_id = group->id;

// 	/* Check whether the group exist or not is withinthe function  */
// 	if (old_group) {
// 		signal_to_group(SIGSTOP, old_group->id);
// 		print_tma_metrics(group_id,
// 				  (struct tma_profile *)old_group->data);
// 	}

// 	signal_to_group(SIGCONT, group_id);
// 	return true;
// }

void schedule_all_groups(void)
{
	signal_to_all_groups(SIGCONT);
}

//Timer callback function
static enum hrtimer_restart hrtimer_hander(struct hrtimer *timer)
{
	if (schedule_next_group()) {
		/* If restarting the next wakeup should be moved forward */
		hrtimer_forward_now(
			timer,
			kt_interval); // hrtimer_forward(timer, now, tick_period);
		return HRTIMER_RESTART; //restart timer
	}

	pr_warn("Schedule didn't get the next group. Stop timer\n");
	return HRTIMER_NORESTART;
}

void init_schedule_tick(void)
{
	// kt_interval = ktime_set(0, 100000000); // 0s 100000000ns = 100ms
	kt_interval = ktime_set(3, 100000000); // 0s 100000000ns = 100ms
	hrtimer_init(&timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL_SOFT);
	timer.function = hrtimer_hander;
	init = true;
}

void enable_scheduler(void)
{
	if (!init)
		init_schedule_tick();

	hrtimer_start(&timer, kt_interval, HRTIMER_MODE_REL_SOFT);
}

void disable_scheduler(void)
{
	hrtimer_try_to_cancel(&timer);
	init = false;
}