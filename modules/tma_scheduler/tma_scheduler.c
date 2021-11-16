// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note

#include <linux/module.h>
#include <linux/moduleparam.h>

#include "device/proc.h"
#include "recode.h"

#include "hooks.h"
#include "plugins/recode_tma.h"
#include "tma_scheduler.h"

enum recode_state __read_mostly recode_state = OFF;

void recode_set_state(uint state)
{
	if (recode_state == state)
		return;

	switch (state) {
	case OFF:
		pr_info("Recode state: OFF\n");
		recode_state = state;
		disable_pmcs_global();
		return;
	case SYSTEM:
		pr_info("Recode ready for SYSTEM\n");
		break;
	default:
		pr_warn("Recode invalid state\n");
		return;
	}

	recode_state = state;
}

/* Register process to activity profiler  */
int attach_process(struct task_struct *tsk)
{
	int err;
	struct tma_profile *profile;

	/* Create group and get payload*/
	profile = (struct tma_profile *)create_group(tsk,
						   sizeof(struct tma_profile));

	if (!profile)
		goto no_payload;

	/* atomic64_t files should be initialized to 0 at allocation */

	err = track_thread(tsk);

	if (err)
no_payload:
		pr_info("Error (%d) while Attaching process: [%u:%u]\n", err,
			tsk->tgid, tsk->pid);
	else
		pr_info("Attaching process: [%u:%u]\n", tsk->tgid, tsk->pid);

	return err;
}

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

static void hook_proc_exit(ARGS_PROC_EXIT)
{
	/* TODO This should change to the last process that leaves the group */
	if (p && (p->pid == p->tgid))
		destroy_group(p);
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
	unregister_hook(PROC_EXIT, hook_proc_exit);
	unregister_hook(SCHED_IN, hook_sched_in);
}

static void on_pmi_callback(uint cpu, struct pmus_metadata *pmus_metadata)
{

	struct tma_collection *tma_collection;
	struct pmcs_collection *pmcs_collection;

	// atomic_inc(&pmis);

	tma_collection = has_group_payload(current);

	if (!tma_collection)
		return;

	/* pmcs_collection should be correct as long as it accessed here */
	pmcs_collection = pmus_metadata->pmcs_collection;

	if (unlikely(!pmcs_collection)) {
		pr_debug("Got a NULL COLLECTION inside PMI\n");
		return;
	}

	pr_debug("Got PMI on TMA\n");

	// u64 mask = pmcs_collection->mask;

	compute_tma_metrics_smp(pmcs_collection, tma_collection);
}

static __init int recode_init(void)
{
	int err = 0;

	pr_info("Mounting with TMA module\n");

	err = recode_groups_init();
	if (err) {
		pr_err("Cannot initialize groups\n");
		goto no_groups;
	}

	err = register_system_hooks();
	if (err)
		goto no_hooks;

	err = recode_init_proc();
	if (err)
		goto no_proc;

	err = recode_tma_init();
	if (err)
		goto no_tma;

	register_on_pmi_callback(on_pmi_callback);

	/* Enable PMU module support */
	pmudrv_set_state(true);

	pr_info("Module loaded\n");
	return err;
no_tma:
	recode_fini_proc();
no_proc:
	unregister_system_hooks();
no_hooks:
	recode_groups_fini();
no_groups:
	return err;
}

static void __exit recode_exit(void)
{
	/* Unregister callback */
	register_on_pmi_callback(NULL);

	recode_tma_fini();

	recode_fini_proc();
	unregister_system_hooks();

	recode_groups_fini();

	pr_info("Module removed\n");
}

module_init(recode_init);
module_exit(recode_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Stefano Carna'");
