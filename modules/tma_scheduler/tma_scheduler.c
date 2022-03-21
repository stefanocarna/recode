// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
#include "linux/slab.h"

#include "recode.h"
#include "pmu_abi.h"

#include "tma_scheduler.h"

enum scheduler_state scheduler_state = SCHEDULER_OFF;

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

enum recode_state_custom { KILL = IDLE + 1 };

int rf_set_state_custom(int old_state, int state)
{
	if (state == KILL) {
		signal_to_all_groups(SIGKILL);
		rf_set_state_off(old_state);
		return 0;
	}

	pr_warn("Recode invalid state\n");
	return -1;
}

void rf_set_state_off(int old_state)
{
	pr_info("Recode state: OFF\n");
	pmudrv_set_state(false);
	disable_scheduler();
}

void rf_set_state_system(int old_state)
{
	// if (!nr_groups) {
	// 	pr_warn("Cannot enable Recode withoutr groups\n");
	// } else {
		pr_info("Recode ready for SYSTEM\n");
		pmudrv_set_state(true);
		enable_scheduler();
	// }
}

int recode_set_scheduler(enum scheduler_state state)
{
	scheduler_state = state;

	if (state != SCHEDULER_OFF && state != SCHEDULER_COLLECT && recode_state == OFF) {
		pr_warn("Recode must be active to enable the scheduler\n");
		return -EINVAL;
	}

	switch (state) {
	case SCHEDULER_OFF:
		disable_scheduler();
		break;
	case SCHEDULER_COLLECT:
		break;
	case SCHEDULER_DIRECT:
		enable_scheduler();
		break;
	case SCHEDULER_DYNAMIC:
	default:
		/* TODO */
		pr_info("Scheduler state %u to be implemented\n", state);
		scheduler_state = SCHEDULER_OFF;
	}

	return 0;
}

/* TODO integrate into PoP */
/* Register process to activity profiler  */
int attach_app(struct task_struct *tsk, char *gname)
{
	return attach_process(tsk, gname);
}

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
	if (scheduler_state == SCHEDULER_COLLECT)
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

void rf_on_pmi_callback(uint cpu, struct pmus_metadata *pmus_metadata)
{
	// u64 sound = 0;
	struct group_entity *group;
	struct tma_profile *profile;
	struct pmcs_collection *pmcs_collection;
	struct tma_collection *tma_collection;

	/* pmcs_collection should be correct as long as it accessed here */
	tma_collection = this_cpu_read(pcpu_tma_collection);

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
	compute_tma_histotrack_smp(pmcs_collection, tma_collection,
				   profile->histotrack,
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