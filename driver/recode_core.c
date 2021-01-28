
#include <asm/ptrace.h>
#include <asm/fast_irq.h>
#include <asm/msr.h>

#include <linux/dynamic-mitigations.h>
#include <linux/percpu-defs.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/stringhash.h>

#include "dependencies.h"
#include "device/proc.h"
#include "recode.h"

DEFINE_PER_CPU(struct pmcs_snapshot, pcpu_pmcs_snapshot) = { 0 };
DEFINE_PER_CPU(bool, pcpu_last_ctx_snapshot) = false;

atomic_t active_pmis;

enum recode_state __read_mostly recode_state = OFF;

#define NR_THRESHOLDS 5
/* The last value is the number of samples while tuning the system */
s64 thresholds[NR_THRESHOLDS + 1] = { 950, 950, 0, 0, 950, 0 };

#define DEFAULT_TS_PRECISION 1000

unsigned ts_precision = DEFAULT_TS_PRECISION;
unsigned ts_precision_5 = (DEFAULT_TS_PRECISION * 0.05);
unsigned ts_window = 5;
unsigned ts_alpha = 1;
unsigned ts_beta = 1;

struct pmc_evt_sel pmc_cfgs[8];

DEFINE_PER_CPU(struct pmcs_snapshot, pcpu_pmc_snapshot_ctx);
DEFINE_PER_CPU(struct pmc_logger *, pcpu_pmc_logger);
DEFINE_PER_CPU(u64, pcpu_counter);

// static bool sampling_enabled = false;
// static bool ais_buffer_full = false;

int recode_data_init(void)
{
	unsigned cpu;

	for_each_online_cpu (cpu) {
		per_cpu(pcpu_pmc_logger, cpu) = init_logger(cpu);
		if (!per_cpu(pcpu_pmc_logger, cpu))
			goto mem_err;
		per_cpu(pcpu_pmcs_active, cpu) = false;
		per_cpu(pcpu_counter, cpu) = 0;
		per_cpu(pcpu_pmcs_snapshot.fixed[fixed_pmc_pmi], cpu) =
			PMC_TRIM(reset_period);
	}

	return 0;
mem_err:
	pr_info("failed to allocate percpu pcpu_pmc_buffer\n");
	return -1;
}

void recode_data_fini(void)
{
	unsigned cpu;

	for_each_online_cpu (cpu) {
		fini_logger(per_cpu(pcpu_pmc_logger, cpu));
	}
}

void recode_pmc_configure(pmc_evt_code *codes)
{
	unsigned k;

	for (k = 0; k < max_pmc_general; ++k) {
		pmc_cfgs[k].perf_evt_sel =
			((u16)codes[k]) | ((codes[k] << 8) & 0xFF000000);
		/* PMCs setup */
		pmc_cfgs[k].usr = 1;
		pmc_cfgs[k].os = 1;
		pmc_cfgs[k].pmi = 0;
		pmc_cfgs[k].en = 1;

		pr_info("recode_pmc_init %llx\n", pmc_cfgs[k].perf_evt_sel);
	}
}

int recode_pmc_init(void)
{
	int irq = 0;
	/* Setup fast IRQ */
	irq = request_fast_irq(RECODE_PMI, pmi_recode);

	if (irq != RECODE_PMI)
		return -1;

	/* READ MACHINE CONFIGURATION */
	get_machine_configuration();

	recode_pmc_configure(pmc_events_sc_detection);

	/* Enable Recode */
	recode_state = OFF;

	return 0;
}

void recode_pmc_fini(void)
{
	/* Disable Recode */
	recode_state = OFF;

	disable_pmc_on_system();

	/* Wait for all PMIs to be completed */
	while (atomic_read(&active_pmis))
		;

	free_fast_irq(RECODE_PMI);
}

void tuning_finish_callback(void *dummy)
{
	unsigned k;

	// recode_set_state(OFF);

	pr_warn("Tuning finished\n");
	pr_warn("Got %llu samples\n", thresholds[NR_THRESHOLDS]);
	pr_warn("Reset period %llx\n", PMC_TRIM(~reset_period));

	for (k = 0; k < NR_THRESHOLDS; ++k) {
		u64 backup = thresholds[k];
		thresholds[k] /= thresholds[NR_THRESHOLDS];
		pr_warn("TS[%u]: %lld/%u (%llu)\n", k, thresholds[k],
			ts_precision, backup);
	}

	pr_warn("Tuning finished\n");
}

static void recode_reset_data(void)
{
	unsigned cpu;
	unsigned pmc;

	for_each_online_cpu (cpu) {
		reset_logger(per_cpu(pcpu_pmc_logger, cpu));
		per_cpu(pcpu_counter, cpu) = 0;

		for (pmc = 0; pmc < max_pmc_fixed; ++pmc)
			per_cpu(pcpu_pmcs_snapshot.fixed[pmc], cpu) = 0;

		per_cpu(pcpu_pmcs_snapshot.fixed[fixed_pmc_pmi], cpu) =
			PMC_TRIM(reset_period);

		for (pmc = 0; pmc < max_pmc_general; ++pmc)
			per_cpu(pcpu_pmcs_snapshot.general[pmc], cpu) = 0;
	}
}

void recode_set_state(unsigned state)
{
	enum recode_state old_state = recode_state;
	recode_state = state;

	if (old_state == recode_state)
		return;

	if (state == OFF) {
		disable_pmc_on_system();
		pr_info("Recode state: OFF\n");
		return;
	} else if (state == TUNING) {
		/* Requires binding to one cpu */
		/* Set threshold mode and activate PROFILE */
		unsigned k = NR_THRESHOLDS + 1;
		while (k--) {
			thresholds[k] = 0;
		}
		set_exit_callback(tuning_finish_callback);
		pr_warn("Recode ready for TUNING\n");
	} else if (state == PROFILE) {
		/* Reset DATA and set PROFILE mode */
		recode_reset_data();
		set_exit_callback(NULL);
		pr_info("Recode ready for PROFILE\n");
	} else if (state == SYSTEM) {
		/* Reset DATA and set SYSTEM mode */
		recode_reset_data();
		pr_info("Recode ready for SYSTEM\n");
	} else if (state == IDLE) {
		pr_info("Recode is IDLE\n");
	} else {
		pr_warn("Recode invalid state\n");
		recode_state = old_state;
		return;
	}

	setup_pmc_on_system(pmc_cfgs);
}

static void ctx_hook(struct task_struct *prev, bool prev_on, bool curr_on)
{
	/* The system is alive, let inform the PMI handler */
	this_cpu_write(pcpu_pmi_counter, 0);
	
	if (recode_state == SYSTEM || recode_state == IDLE) {
		/* PMCs will be enabled inside pmc_evaluate_activity */
		pmc_evaluate_activity(prev, prev_on, true);
	} else if (recode_state == OFF) {
		/* This is redundant, but it improves safety */
		if (this_cpu_read(pcpu_pmcs_active))
			disable_pmc_on_cpu();
		return;
	} else { 
		/* PROFILE || TUNING */
		/* Toggle PMI */
		if (this_cpu_read(pcpu_pmcs_active) && !curr_on)
			disable_pmc_on_cpu();

		else if (!this_cpu_read(pcpu_pmcs_active) && curr_on)
			// enable_pmc_on_cpu();
			/* PMCs will be enabled inside pmc_evaluate_activity */
			pmc_evaluate_activity(prev, prev_on, true);
			
		else if (this_cpu_read(pcpu_pmcs_active))
			pmc_evaluate_activity(prev, prev_on, true);
	}

	/* Activate on previous task */
	if (has_pending_mitigations(prev))
		enable_mitigations_on_task(prev);

	/* Enable mitigations */
	LLC_flush(current);
	mitigations_switch(prev, current);
}

static bool evaluate_pmcs(struct task_struct *tsk,
			  struct pmcs_snapshot *snapshot)
{
	if (recode_state == IDLE) {
		/* Do nothing */
	} else if (recode_state == TUNING) {
		thresholds[0] += DM0(ts_precision, snapshot);
		thresholds[1] += DM1(ts_precision, snapshot);
		thresholds[2] += DM2(ts_precision, snapshot);
		thresholds[3] += DM3(ts_precision, snapshot);
		// thresholds[4] += DM4(ts_precision, snapshot);
		thresholds[NR_THRESHOLDS]++;

		pr_warn("Got sample %llu\n", thresholds[0]);
	} else {
		if (!has_mitigations(tsk)) {
			// TODO Remove - Init task_struct
			if (tsk->monitor_state > ts_window + 1)
				tsk->monitor_state = 0;

			if ((CHECK_LESS_THAN_TS(thresholds[0],
						DM0(ts_precision, snapshot),
						ts_precision_5) &&
			     CHECK_LESS_THAN_TS(thresholds[1],
						DM1(ts_precision, snapshot),
						ts_precision_5) &&
			     CHECK_MORE_THAN_TS(thresholds[2],
						DM2(ts_precision, snapshot),
						ts_precision_5) &&
			     CHECK_MORE_THAN_TS(thresholds[3],
						DM3(ts_precision, snapshot),
						ts_precision_5)) ||
			    /* P4 */
			    CHECK_LESS_THAN_TS(thresholds[4],
					       DM3(ts_precision, snapshot),
					       ts_precision_5)) {
				tsk->monitor_state += ts_alpha;
				pr_info("[++] %s (PID %u): %u\n", tsk->comm,
					tsk->pid, tsk->monitor_state);
			} else if (tsk->monitor_state > 0) {
				tsk->monitor_state -= ts_alpha;
				pr_info("[--] %s (PID %u): %u\n", tsk->comm,
					tsk->pid, tsk->monitor_state);
			}

			if (tsk->monitor_state > ts_window) {
				pr_warn("[FLAG] Detected %s (PID %u): %u\n",
					tsk->comm, tsk->pid,
					tsk->monitor_state);
				pr_warn("0: %llu, 1: %llu, 2: %llu, 3/4: %llu\n",
					DM0(ts_precision, snapshot),
					DM1(ts_precision, snapshot),
					DM2(ts_precision, snapshot),
					DM3(ts_precision, snapshot));
				return true;
			}
		}
	}
	return false;
}

void pmc_evaluate_activity(struct task_struct *tsk, bool log, bool pmc_off)
{
	unsigned k;
	unsigned cpu = get_cpu();
	pmc_ctr old_fixed1, new_fixed1;
	struct pmcs_snapshot old_pmcs;

	if (pmc_off)
		disable_pmc_on_cpu();

	/* TODO - This copy is implementation-dependant */
	memcpy(&old_pmcs, per_cpu_ptr(&pcpu_pmcs_snapshot, cpu),
	       sizeof(struct pmcs_snapshot));

	/* Read PMCs' value */
	read_all_pmcs(per_cpu_ptr(&pcpu_pmcs_snapshot, cpu));

	/* 
	 * What if a PMI occurs and the IRQs are OFF?
	 * 
	 * This function is executed at the end of teh context switch and in the
	 * PMI routine. The context switch may occur while the PMC overflows and
	 * a wrong value would be read. We need to adjust such a value with the
	 * reset_period.
	 * 
	 * 1. Do we need to check if there is a pending PMI?
	 * 2. Can we just look for and older value bigger than the new one? 
	 */

	/* Implementing Solution 2. */
	old_fixed1 = old_pmcs.fixed[fixed_pmc_pmi];
	new_fixed1 = per_cpu(pcpu_pmcs_snapshot.fixed[fixed_pmc_pmi], cpu);

	for_each_pmc(k, max_pmc_fixed + max_pmc_general)
	{
		old_pmcs.pmcs[k] =
			PMC_TRIM(per_cpu(pcpu_pmcs_snapshot.pmcs[k], cpu) -
				 old_pmcs.pmcs[k]);
	}

	/* TSC is not computed and it is set to "this" moment */
	old_pmcs.tsc = per_cpu(pcpu_pmcs_snapshot.tsc, cpu);

	if (old_fixed1 >= new_fixed1) {
		old_pmcs.fixed[1] =
			PMC_TRIM(((BIT_ULL(48) - 1) - old_fixed1) + new_fixed1);
		this_cpu_add(pcpu_pmcs_snapshot.fixed[fixed_pmc_pmi],
			     reset_period + new_fixed1);
	} else {
		old_pmcs.fixed[fixed_pmc_pmi] = new_fixed1 - old_fixed1;
	}
	
	// goto end;

	if (log)
		log_sample(per_cpu(pcpu_pmc_logger, cpu), &old_pmcs);

	/* Skip small samples */
	if ((per_cpu(pcpu_pmcs_snapshot.fixed[fixed_pmc_pmi], cpu) - 
	    old_pmcs.fixed[fixed_pmc_pmi]) > 0xFFFF)
		if(evaluate_pmcs(tsk, &old_pmcs))
			/* Delay activation if we are inside the PMI */
			request_mitigations_on_task(tsk, pmc_off);

// end:
	if (pmc_off)
		enable_pmc_on_cpu();
	put_cpu();
}

/* Register thread for profiling activity  */
int attach_process(pid_t pid)
{
	pr_info("Attaching pid %u\n", pid);

	pid_register(pid);

	return 0;
}

/* Remove registered thread from profiling activity */
void detach_process(pid_t pid)
{
	// TODO
}

int register_ctx_hook(void)
{
	int err = 0;

	hook_register(&ctx_hook);
	// pid_register(16698);
	switch_hook_resume();
	// test_asm();
	return err;
}

void unregister_ctx_hook(void)
{
	hook_unregister();
}
