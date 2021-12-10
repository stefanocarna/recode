
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

/* TODO - create a module */
#ifdef TMA_MODULE_ON
#include "logic/recode_tma.h"
#endif
#ifdef SECURITY_MODULE_ON
#include "logic/recode_security.h"
#endif

#include "hooks.h"

bool dummy_on_state_change(enum recode_state state)
{
	return false;
}

static void dummy_on_ctx(struct task_struct *prev, struct task_struct *next)
{
	// Empty call
}

// static void dummy_on_hw_events_change(struct hw_events *events)
// {
// 	// Empty call
// }

struct recode_callbacks recode_callbacks = {
	// .on_hw_events_change = dummy_on_hw_events_change,
	.on_ctx = dummy_on_ctx,
	.on_state_change = dummy_on_state_change,
};

enum recode_state __read_mostly recode_state = OFF;

DEFINE_PER_CPU(struct data_logger *, pcpu_data_logger);

int recode_data_init(void)
{
	unsigned cpu;

	for_each_online_cpu (cpu) {
		per_cpu(pcpu_data_collector, cpu) = init_collector(cpu);
		if (!per_cpu(pcpu_data_collector, cpu))
			goto mem_err;
	}

	return 0;
mem_err:
	pr_info("failed to allocate percpu pcpu_pmc_buffer\n");

	while (--cpu)
		fini_collector(cpu);

	return -1;
}

void recode_data_fini(void)
{
	unsigned cpu;

	for_each_online_cpu (cpu) {
		fini_collector(cpu);
	}
}

int recode_pmc_init(void)
{
	return 0;
}

void recode_pmc_fini(void)
{
	// pr_info("PMU uninstalled\n");
}

/* Must be implemented */
static void recode_reset_data(void)
{

}

#define ARGS_DATA(args...) void *data, args
#define ARGS_SCHED_IN                                                          \
	ARGS_DATA(bool preempt, struct task_struct *prev,                      \
		  struct task_struct *next)
#define ARGS_PROC_EXIT ARGS_DATA(struct task_struct *p)

static void recode_hook_sched_in(ARGS_SCHED_IN)
{
	uint cpu = get_cpu();
	// struct data_logger_sample sample;

	switch (recode_state) {
	case OFF:
		disable_pmcs_local(false);
		goto end;
	case IDLE:
		enable_pmcs_local(false);
		break;
	case SYSTEM:
		enable_pmcs_local(false);
		// prev_on = true;
		break;
	case TUNING:
		fallthrough;
	case PROFILE:
		/* Toggle PMI */
		// if (!curr_on)
		// 	disable_pmcs_local(false);
		// else
		// 	enable_pmcs_local(false);
		break;
	default:
		pr_warn("Invalid state on cpu %u... disabling PMCs\n", cpu);
		disable_pmcs_local(false);
		goto end;
	}

	// if (unlikely(!prev)) {
	// 	goto end;
	// }

	// TODO Remove
	// if (query_tracked(next)) {
	// 	pr_info("%u] vrt: %llu, TSC: %llu", prev->pid, prev->se.vruntime, rdtsc_ordered());
	// }



	recode_callbacks.on_ctx(prev, next);

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

end:
	put_cpu();
}

static void manage_pmu_state(void *dummy)
{
	/* This is not precise */
	// TODO CHECK
	// this_cpu_write(pcpu_pmus_metadata.last_tsc, rdtsc_ordered());

	// TODO RESTORE
	// on_context_switch_callback(NULL, false, query_tracker(current));
}

void recode_set_state(uint state)
{
	if (recode_state == state)
		return;

	if (recode_callbacks.on_state_change(state))
		goto skip;

	switch (state) {
	case OFF:
		pr_info("Recode state: OFF\n");
		recode_state = state;
		disable_pmcs_global();
		// TODO - Restore
		// flush_written_samples_global();
		return;
	case IDLE:
		pr_info("Recode ready for IDLE\n");
		break;
	case PROFILE:
		pr_info("Recode ready for PROFILE\n");
		break;
	case SYSTEM:
		pr_info("Recode ready for SYSTEM\n");
		break;
	case TUNING:
		pr_info("Recode ready for TUNING\n");
		break;
	default:
		pr_warn("Recode invalid state\n");
		return;
	}

skip:
	recode_state = state;

	recode_reset_data();
	on_each_cpu(manage_pmu_state, NULL, 0);

	/* Use the cached value */
	/* TODO - check pmc setup methods */
	// setup_pmc_global(NULL);
	// TODO - Make this call clear
}

/* Register process to activity profiler  */
int attach_process(struct task_struct *tsk, char *gname)
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
void detach_process(struct task_struct *tsk)
{
	pr_info("Detaching process: [%u:%u]\n", tsk->tgid, tsk->pid);
	untrack_thread(tsk);
}

int register_system_hooks(void)
{
	int err = 0;

	err = register_hook(SCHED_IN, recode_hook_sched_in);

	if (err) {
		pr_info("Cannot register SCHED_IN callback\n");
		goto end;
	} else {
		pr_info("Registered SCHED_IN callback\n");
	}

end:
	return err;
}

void unregister_system_hooks(void)
{
	unregister_hook(SCHED_IN, recode_hook_sched_in);
}

// void setup_hw_events_from_proc(pmc_evt_code *hw_events_codes, unsigned cnt)
// {
// 	if (!hw_events_codes || !cnt) {
// 		pr_warn("Invalid hw evenyts codes from user\n");
// 		return;
// 	}

// 	setup_hw_events_global(hw_events_codes, cnt);
// }
