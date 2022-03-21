// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note

#include "hooks.h"
#include "recode.h"
#include "logic/tma.h"
#include "tma_scheduler.h"

#include <linux/slab.h>

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
	struct tma_profile *profile;

	group = get_group_by_proc(p->pid);

	if (!group)
		return;

	profile = unregister_process_from_group(p->pid, group);

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
