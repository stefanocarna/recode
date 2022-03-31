
#ifdef FAST_IRQ_ENABLED
#include <asm/fast_irq.h>
#endif
#include <asm/msr.h>

#include <linux/percpu-defs.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/stringhash.h>

#include <asm/tsc.h>

#include "pmu_abi.h"

#include "device/proc.h"

#include "recode.h"
#include "recode_config.h"
#include "recode_collector.h"
#include "recode_memory.h"
#include "recode_groups.h"

/* TODO - create a module */

#ifdef TMA_MODULE_ON
#include "logic/recode_tma.h"
#endif
#ifdef SECURITY_MODULE_ON
#include "logic/recode_security.h"
#endif

bool buffering_state = true;
bool buffering_deep_state = true;

// __weak void rf_on_pmi_callback(uint cpu, struct pmus_metadata *pmus_metadata)
// {
// 	int cnt;
// 	int mem_size;
// 	struct proc_entity *pentity;
// 	struct stats_sample *ssample;

// 	/* Nothing to do */
// 	if (recode_state == OFF)
// 		return;

// 	// TODO @change to private_data access
// 	pentity = get_proc_by_pid(current->pid);
// 	if (!pentity)
// 		return;

// 	if (buffering_state) {
// 		if (tma_enabled) {
// 			cnt = this_cpu_read(pcpu_tma_collection)->cnt;
// 			mem_size = sizeof(struct stats_sample *) +
// 				   array_size(sizeof(u64), cnt);
// 			ssample = (struct stats_sample *)
// 				get_from_memory_bulk_local(mem_size);

// 			/* No Memory */
// 			if (!ssample)
// 				return;

// 			ssample->cpu = cpu;

// 			// pr_info("%lld\n", this_cpu_ptr(pcpu_tma_collection.metrics)[0]);
// 			/* Copy raw pmc values */
// 			/* TODO Fix control check */
// 			memcpy(&ssample->tma,
// 			       this_cpu_read(pcpu_tma_collection),
// 			       sizeof(struct tma_collection) +
// 				       array_size(sizeof(u64), cnt));

// 			ssample->tma_level =
// 				this_cpu_read(pcpu_tma_collection)->level;
// 		} else {
// 			/* UNSUPPORTED */

// 			// cnt = pmus_metadata->pmcs_collection->cnt;
// 			// mem_size = sizeof(*tma_collection) + array_size(sizeof(u64), cnt);
// 			// dc_sample = (struct data_collector_sample*) get_from_memory_bulk_local(mem_size);

// 			// dc_sample = get_write_dc_sample(dc, cnt, 0);
// 			// /* Copy raw pmc values */
// 			// memcpy(&dc_sample->pmcs, pmus_metadata->pmcs_collection,
// 			//        sizeof(dc_sample->pmcs) +
// 			// 	       array_size(sizeof(pmc_ctr), cnt));

// 			// dc_sample->tma_level = -1;
// 		}

// 		if (buffering_deep_state) {
// 			// dc_sample->id = current->pid;
// 			// dc_sample->tracked = query_tracked(current);
// 			// dc_sample->k_thread = !current->mm;

// 			// TODO make this consistent with TMA stop

// 			ssample->system_tsc = pmus_metadata->last_tsc;
// 			ssample->tsc_cycles = pmus_metadata->sample_tsc;
// 			// dc_sample->core_cycles =
// 			// 	pmcs_fixed(pmus_metadata->pmcs_collection->pmcs)[1];
// 			// dc_sample->core_cycles_tsc_ref =
// 			// 	pmcs_fixed(pmus_metadata->pmcs_collection->pmcs)[2];
// 			// dc_sample->ctx_evts = pmus_metadata->ctx_evts;

// 			// get_task_comm(dc_sample->task_name, current);
// 		}

// 		// put_write_dc_sample(this_cpu_read(pcpu_data_collector));
// 	}

// 	// TODO Create a dedicated function
// 	if (!pentity->stats.samples_tail) {
// 		pentity->stats.samples_head = ssample;
// 		pentity->stats.samples_tail = ssample;
// 	} else {
// 		pentity->stats.samples_tail->next = ssample;
// 		pentity->stats.samples_tail = ssample;
// 	}

// 	pentity->stats.nr_samples++;

// 	// if (query_tracked(current)) {
// 	// 	atomic_inc(&tracked_pmi);
// 	// }
// }

__weak void rf_on_pmi_callback(uint cpu, struct pmus_metadata *pmus_metadata)
{
	int cnt;
	int mem_size;
	struct proc_entity *pentity;
	struct stats_sample *ssample;

	/* Nothing to do */
	if (recode_state == OFF)
		return;

	// TODO @change to private_data access
	pentity = get_proc_by_pid(current->pid);
	if (!pentity)
		return;

	if (buffering_state) {
		if (tma_enabled) {
			cnt = this_cpu_read(pcpu_tma_collection)->cnt;
			mem_size = sizeof(struct stats_sample *) +
				   array_size(sizeof(u64), cnt);
			ssample = (struct stats_sample *)
				get_from_memory_bulk_local(mem_size);

			/* No Memory */
			if (!ssample)
				return;

			ssample->cpu = cpu;

			// pr_info("%lld\n", this_cpu_ptr(pcpu_tma_collection.metrics)[0]);
			/* Copy raw pmc values */
			/* TODO Fix control check */
			memcpy(&ssample->tma,
			       this_cpu_read(pcpu_tma_collection),
			       sizeof(struct tma_collection) +
				       array_size(sizeof(u64), cnt));

			ssample->tma_level =
				this_cpu_read(pcpu_tma_collection)->level;

			atomic_add(&pentity->group->stats.retire,
				   ssample->tma.metrics[2]);
			atomic_inc(&pentity->group->stats.retire_counter);

			pentity->stats.retire += ssample->tma.metrics[2];
		} else {
			/* UNSUPPORTED */

			// cnt = pmus_metadata->pmcs_collection->cnt;
			// mem_size = sizeof(*tma_collection) + array_size(sizeof(u64), cnt);
			// dc_sample = (struct data_collector_sample*) get_from_memory_bulk_local(mem_size);

			// dc_sample = get_write_dc_sample(dc, cnt, 0);
			// /* Copy raw pmc values */
			// memcpy(&dc_sample->pmcs, pmus_metadata->pmcs_collection,
			//        sizeof(dc_sample->pmcs) +
			// 	       array_size(sizeof(pmc_ctr), cnt));

			// dc_sample->tma_level = -1;
		}

		if (buffering_deep_state) {
			// dc_sample->id = current->pid;
			// dc_sample->tracked = query_tracked(current);
			// dc_sample->k_thread = !current->mm;

			// TODO make this consistent with TMA stop

			ssample->system_tsc = pmus_metadata->last_tsc;
			ssample->tsc_cycles = pmus_metadata->sample_tsc;
			// dc_sample->core_cycles =
			// 	pmcs_fixed(pmus_metadata->pmcs_collection->pmcs)[1];
			// dc_sample->core_cycles_tsc_ref =
			// 	pmcs_fixed(pmus_metadata->pmcs_collection->pmcs)[2];
			// dc_sample->ctx_evts = pmus_metadata->ctx_evts;

			// get_task_comm(dc_sample->task_name, current);
		}

		// put_write_dc_sample(this_cpu_read(pcpu_data_collector));
	}

	// TODO Create a dedicated function
	if (!pentity->stats.samples_tail) {
		pentity->stats.samples_head = ssample;
		pentity->stats.samples_tail = ssample;
	} else {
		pentity->stats.samples_tail->next = ssample;
		pentity->stats.samples_tail = ssample;
	}

	pentity->stats.nr_samples++;

	// if (query_tracked(current)) {
	// 	atomic_inc(&tracked_pmi);
	// }
}

enum recode_state __read_mostly recode_state = OFF;

DEFINE_PER_CPU(struct data_logger *, pcpu_data_logger);

int recode_data_init(void)
{
	int cpu;

	for_each_online_cpu(cpu) {
		per_cpu(pcpu_memory_bulk_manager, cpu) =
			init_memory_bulk_menager(cpu);
		if (!per_cpu(pcpu_memory_bulk_manager, cpu))
			goto mem_err;

		// per_cpu(pcpu_data_collector, cpu) = init_collector(cpu);
		// if (!per_cpu(pcpu_data_collector, cpu))
		// 	goto mem_err;
	}

	return 0;
mem_err:
	pr_info("failed to allocate pcpu_memory_bulk_manager\n");

	while (--cpu)
		fini_memory_bulk_menager(cpu);
	// fini_collector(cpu);

	return -1;
}

void recode_data_fini(void)
{
	unsigned cpu;

	for_each_online_cpu(cpu) {
		// fini_collector(cpu);
		fini_memory_bulk_menager(cpu);
	}
}

/* Must be implemented */
static void recode_reset_data(void)
{
}

__weak int rf_hook_sched_in_custom_state(struct task_struct *prev,
					 struct task_struct *next)
{
	pr_warn("Invalid state on cpu %u... disabling PMCs\n",
		smp_processor_id());
	disable_pmcs_local(false);
	return -1;
}

__weak void rf_after_hook_sched_in(struct task_struct *prev,
				   struct task_struct *next)
{
	// struct data_logger_sample sample;

	// per_cpu(pcpu_pmus_metadata.has_ctx_switch, cpu) = true;

	/* TODO - Think aobut enabling or not the sampling in CTX_SWITCH */

	// if (!prev_on) {
	// 	pmc_generate_snapshot(NULL, false);
	// } else {
	// 	pmc_generate_snapshot(&sample.pmcs, true);
	// 	// TODO - Note here pmcs are active, so we are recording
	// 	//        part of the sample collection work
	// 	sample.id = prev->pid;
	// 	sample.tracked = true;
	// 	sample.k_thread = !prev->mm;
	// 	write_log_sample(per_cpu(pcpu_data_logger, cpu), &sample);
	// }
}

static void manage_pmu_state(void *dummy)
{
	/* This is not precise */
	// TODO CHECK
	// this_cpu_write(pcpu_pmus_metadata.last_tsc, rdtsc_ordered());

	// TODO RESTORE
	// on_context_switch_callback(NULL, false, query_tracker(current));
}

__weak void rf_set_state_off(int old_state)
{
	pmudrv_set_state(false);
	pr_info("Recode state: OFF\n");

	// TODO - Restore
	// flush_written_samples_global();
}

__weak void rf_set_state_idle(int old_state)
{
	pmudrv_set_state(true);
	pr_info("Recode ready for IDLE\n");
}

__weak void rf_set_state_profile(int old_state)
{
	pmudrv_set_state(true);
	pr_info("Recode ready for PROFILE\n");
}

__weak void rf_set_state_system(int old_state)
{
	pmudrv_set_state(true);
	pr_info("Recode ready for SYSTEM\n");
}

__weak int rf_set_state_custom(int old_state, int state)
{
	pr_warn("Recode invalid state\n");
	return -1;
}

__weak void rf_before_set_state(int old_state, int state)
{
	/* Nothing to do */
}

void recode_set_state(int state)
{
	if (recode_state == state)
		return;

	rf_before_set_state(recode_state, state);

	switch (state) {
	case OFF:
		recode_state = OFF;
		rf_set_state_off(recode_state);
		return;
	case IDLE:
		rf_set_state_idle(recode_state);
		break;
	case PROFILE:
		rf_set_state_profile(recode_state);
		break;
	case SYSTEM:
		rf_set_state_system(recode_state);
		break;
	default:
		if (rf_set_state_custom(recode_state, state))
			return;
	}

	recode_state = state;

	recode_reset_data();
	on_each_cpu(manage_pmu_state, NULL, 0);

	/* Use the cached value */
	/* TODO - check pmc setup methods */
	// setup_pmc_global(NULL);
	// TODO - Make this call clear
}

/* Register process to activity profiler  */
__weak int attach_process(struct task_struct *tsk, char *gname)
{
	int err;

	/* Create group and get payload*/
	/*** TODO Restore ***/
	// create_group(tsk, sizeof(struct tma_profile));

	err = track_thread(tsk);

	if (err)
		pr_info("Error (%d) while Attaching process: [%u:%u]\n", err,
			tsk->tgid, tsk->pid);
	else
		pr_info("Attaching process: [%u:%u]\n", tsk->tgid, tsk->pid);

	return err;
}

/* Remove registered thread from profiling activity */
__weak void detach_process(struct task_struct *tsk)
{
	pr_info("Detaching process: [%u:%u]\n", tsk->tgid, tsk->pid);
	untrack_thread(tsk);
}

// void setup_hw_events_from_proc(pmc_evt_code *hw_events_codes, unsigned cnt)
// {
// 	if (!hw_events_codes || !cnt) {
// 		pr_warn("Invalid hw evenyts codes from user\n");
// 		return;
// 	}

// 	setup_hw_events_global(hw_events_codes, cnt);
// }
