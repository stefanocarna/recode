// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note

#include "hooks.h"
#include "recode.h"
#include "plugins/recode_tma.h"

#include "tma_scheduler.h"

#include <linux/slab.h>

static void hook_sched_in(ARGS_SCHED_IN)
{
	uint cpu = get_cpu();

	switch (recode_state) {
	case OFF:
		disable_pmcs_local(false);
		goto end;
	case SYSTEM:
		enable_pmcs_local(false);
		break;
	default:
		pr_warn("Invalid state on cpu %u... disabling PMCs\n", cpu);
		disable_pmcs_local(false);
		goto end;
	}

end:
	put_cpu();
}

static void hook_proc_fork(ARGS_PROC_FORK)
{
	struct group_entity *group;

	group = get_group_by_proc(parent->pid);

	if (group) {
		register_process_to_group(child->pid, group, create_process_profile());
		pr_info("%u:%u group @ fork %u:%u\n", parent->tgid, parent->pid,
			child->tgid, child->pid);
	}
}

static void hook_proc_exit(ARGS_PROC_EXIT)
{
	struct group_entity *group;
	struct tma_profile *profile;

	group = get_group_by_proc(p->pid);

	if (!group)
		return;

	profile = unregister_process_from_group(p->pid, group);

	aggregate_tma_profile(profile, (struct tma_profile *)group->data);

	if (!group->nr_processes) {
		print_tma_metrics(group->id, (struct tma_profile *)group->data);
		destroy_group(group->id);
	}

	pr_info("Exiting %u\n", p->pid);
}

int register_system_hooks(void)
{
	int err = 0;

	err = register_hook(SCHED_IN, hook_sched_in);

	if (err) {
		pr_info("Cannot register SCHED_IN callback\n");
		goto end;
	} else {
		pr_info("Registered SCHED_IN callback\n");
	}

	err = register_hook(PROC_FORK, hook_proc_fork);

	if (err) {
		pr_info("Cannot register PROC_FORK callback\n");
		goto end;
	} else {
		pr_info("Registered PROC_FORK callback\n");
	}

	err = register_hook(PROC_EXIT, hook_proc_exit);

	if (err) {
		pr_info("Cannot register PROC_EXIT callback\n");
		goto end;
	} else {
		pr_info("Registered PROC_EXIT callback\n");
	}

end:
	return err;
}

void unregister_system_hooks(void)
{
	unregister_hook(SCHED_IN, hook_sched_in);
	unregister_hook(PROC_FORK, hook_proc_fork);
	unregister_hook(PROC_EXIT, hook_proc_exit);
}