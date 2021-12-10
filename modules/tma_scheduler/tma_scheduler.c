// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
#include "recode.h"
#include "hooks.h"
#include "device/proc.h"

#include "plugins/recode_tma.h"
#include "tma_scheduler.h"

#include <linux/module.h>
#include <linux/moduleparam.h>

enum recode_state __read_mostly recode_state = OFF;

void aggregate_tma_profile(struct tma_profile *proc_profile,
			   struct tma_profile *group_profile)
{
	uint m, k;

	for (m = 0; m < TMA_NR_L3_FORMULAS; ++m)
		for (k = 0; k < TRACK_PRECISION; ++k)
			atomic_add(atomic_read(&proc_profile->histotrack[m][k]),
				   &group_profile->histotrack[m][k]);
}

__always_inline struct tma_profile *create_process_profile(void)
{
	return kmalloc(sizeof(struct tma_profile), GFP_KERNEL);
}

__always_inline void destroy_process_profile(struct tma_profile *profile)
{
	kfree(profile);
}

void print_tma_metrics(uint id, struct tma_profile *profile)
{
	uint k;
	u64 samples;

	if (!profile)
		return;

	samples = atomic_read(&profile->nr_samples);

	pr_info("GROUP: %u - samples %llu\n", id, samples);

#define X_TMA_LEVELS_FORMULAS(name, idx)                                       \
	pr_info("%s:  0  10  20  30  40  50  60  70  80  90\n               ", \
		#name);                                                        \
	for (k = 0; k < TRACK_PRECISION; ++k) {                                \
		pr_cont(" [%u]", atomic_read(&profile->histotrack[idx][k]));   \
	}

	TMA_L3_FORMULAS
#undef X_TMA_LEVELS_FORMULAS
}

void recode_set_state(uint state)
{
	if (recode_state == state)
		return;

	switch (state) {
	case KILL:
		signal_to_all_groups(SIGKILL);
		fallthrough;
	case OFF:
		pr_info("Recode state: OFF\n");
		recode_state = OFF;
		disable_pmcs_global();
		disable_scheduler();
		/* Enable all ? */
		// schedule_all_groups();
		return;
	case SYSTEM:
		if (!nr_groups) {
			pr_warn("Cannot enable Recode withoutr groups\n");
			break;
		}

		enable_scheduler();
		pr_info("Recode ready for SYSTEM\n");
		break;
	default:
		pr_warn("Recode invalid state\n");
		return;
	}

	recode_state = state;
}

/* Register process to activity profiler  */
int attach_process(struct task_struct *tsk, char *gname)
{
	int err = 0;
	void *data;
	struct group_entity *group;

	data = kmalloc(sizeof(struct tma_profile), GFP_KERNEL);

	if (!data)
		goto no_data;

	/* Create group and get data */
	group = create_group(gname, tsk->pid, data);
	if (!group)
		goto no_group;

	err = register_process_to_group(tsk->pid, group,
					create_process_profile());
	if (err)
		goto no_register;

	/* Suspend process here */
	signal_to_group_by_id(SIGSTOP, group->id);

	pr_info("Attaching process: [%u:%u]\n", tsk->tgid, tsk->pid);
	return 0;

no_register:
	destroy_group(tsk->pid);
no_group:
	kfree(data);
no_data:
	pr_info("Error (%d) while Attaching process: [%u:%u]\n", err, tsk->tgid,
		tsk->pid);
	return err;
}

static void on_pmi_callback(uint cpu, struct pmus_metadata *pmus_metadata)
{
	// u64 sound = 0;
	struct group_entity *group;
	struct tma_profile *profile;
	struct pmcs_collection *pmcs_collection;

	// atomic_inc(&pmis);
	// pr_info("Got PMI on TMA: %u:%u\n", current->tgid, current->pid);

	group = get_group_by_proc(current->pid);
	if (!group)
		return;

	profile = (struct tma_profile *)group->data;

	if (!profile)
		return;

	/* pmcs_collection should be correct as long as it accessed here */
	pmcs_collection = pmus_metadata->pmcs_collection;

	if (unlikely(!pmcs_collection)) {
		pr_debug("Got a NULL COLLECTION inside PMI\n");
		return;
	}

	// compute_tma_metrics_smp(pmcs_collection, &profile->tma);
	compute_tma_histotrack_smp(pmcs_collection, profile->histotrack,
				   profile->histotrack_comp,
				   &profile->nr_samples);

	/* TODO Check soundness */

	// if (atomic64_read(&profile->tma.nr_samples)) {
	// 	sound += atomic64_read(&profile->tma.metrics[0]);
	// 	sound += atomic64_read(&profile->tma.metrics[1]);
	// 	sound += atomic64_read(&profile->tma.metrics[2]);
	// 	sound += atomic64_read(&profile->tma.metrics[3]);

	// 	sound /= atomic64_read(&profile->tma.nr_samples);

	// 	/* TODO Remove */
	// 	if (sound != 1000) {
	// 		pr_err("ERR sound: %llu\nRESET: %llx - MULTIPX %u\n",
	// 		       sound, pmus_metadata->pmi_reset_value,
	// 		       pmus_metadata->multiplexing);
	// 	} else {
	// 		// pr_warn("RESET: %llx - MULTIPX %u\n",
	// 		//        pmus_metadata->pmi_reset_value,
	// 		//        pmus_metadata->multiplexing);
	// 	}
	// }
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

	register_proc_group();
	register_proc_csched();

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
	recode_set_state(OFF);

	pmudrv_set_state(false);

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
