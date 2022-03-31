// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note

#include <linux/perf_event.h>

#include "pmu.h"
#include "pmu_core.h"
#include "recode.h"
#include "recode_groups.h"

__weak void pop_hook_sched_in(ARGS_SCHED_IN)
{
	switch (recode_state) {
	case OFF:
		disable_pmcs_local(false);
		return;
	case IDLE:
	case SYSTEM:
		enable_pmcs_local(false);
		break;
	case PROFILE:
		/* Toggle PMI */
		// TODO Adapt query tracker to group
		// if (!query_tracked(next))
		if (!get_group_by_proc(next->pid))
			disable_pmcs_local(false);
		else
			enable_pmcs_local(false);
		break;
	default:
		if (rf_hook_sched_in_custom_state(prev, next))
			return;
	}


	rf_after_hook_sched_in(prev, next);
}

// __weak void pop_hook_proc_fork(ARGS_PROC_FORK)
// {
// 	// if (!pmu_enabled)
// 	// 	return;

// 	preempt_enable_notrace();

// 	if (get_task_data(parent))
// 		create_task_data(child);

// 	preempt_disable_notrace();
// }


// __weak void pop_hook_proc_exit(ARGS_PROC_EXIT)
// {
// 	destroy_task_data(p);
// }



void pop_hook_proc_fork(ARGS_PROC_FORK)
{
	struct group_entity *group;

	group = get_group_by_proc(parent->pid);

	if (group) {
		// register_process_to_group(child->pid, group,
		// 			  create_process_profile());
		register_process_to_group(child->pid, group,
					  (struct tma_profile *) group->data);
		pr_debug("%u:%u group @ fork %u:%u\n", parent->tgid, parent->pid,
			child->tgid, child->pid);
	}
}

void pop_hook_proc_exit(ARGS_PROC_EXIT)
{
	// We borrow the *data parameter
	// void *data;
	struct group_entity *group;
	// struct tma_profile *profile;

	// TODO Renable

	// group = get_group_by_proc(p->pid);

	// if (!group)
	// 	return;

	// profile = unregister_process_from_group(p->pid, group);

	/* TODO modified - TMA data is shared at group level */
	// aggregate_tma_profile(profile, (struct tma_profile *)group->data);
	pr_debug("would aggregate p %u to g %u\n", p->pid, group->id);

	// TODO Destroy group only manually
	// if (!group->nr_processes) {
	// 	// print_tma_metrics(group->id, (struct tma_profile *)group->data);
	// 	data = destroy_group(group->id);
	// 	kfree(data);
	// }

	pr_debug("Exiting %u\n", p->pid);
}


struct hook_func {
	void *func;
	enum hook_type type;
};

static struct hook_func hook_funcs[] = { { pop_hook_sched_in, SCHED_IN },
					 { pop_hook_proc_fork, PROC_FORK },
					 { pop_hook_proc_exit, PROC_EXIT } };

int system_hooks_init(void)
{
	uint i;

	for (i = 0; i < ARRAY_SIZE(hook_funcs); ++i) {
		if (register_hook(hook_funcs[i].type, hook_funcs[i].func))
			goto no_hooks;
	}

	return 0;

no_hooks:
	for (i = i - 1; i >= 0; --i)
		unregister_hook(hook_funcs[i].type, hook_funcs[i].func);
}

void system_hooks_fini(void)
{
	uint i;

	for (i = 0; i < ARRAY_SIZE(hook_funcs); ++i)
		unregister_hook(hook_funcs[i].type, hook_funcs[i].func);
}