
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

#include "dependencies.h"
#include "device/proc.h"

#include "recode.h"
#include "pmu/pmi.h"
#include "pmu/pmu.h"
#include "recode_config.h"

DEFINE_PER_CPU(struct pmcs_snapshot, pcpu_pmcs_snapshot) = { 0 };
DEFINE_PER_CPU(bool, pcpu_last_ctx_snapshot) = false;

enum recode_state __read_mostly recode_state = OFF;

atomic_t on_samples_flushing = ATOMIC_INIT(0);

DEFINE_PER_CPU(struct pmcs_snapshot, pcpu_pmc_snapshot_ctx);
DEFINE_PER_CPU(struct pmc_logger *, pcpu_pmc_logger);


int recode_data_init(void)
{
	unsigned cpu;

	for_each_online_cpu (cpu) {
		per_cpu(pcpu_pmc_logger, cpu) = init_logger(cpu);
		if (!per_cpu(pcpu_pmc_logger, cpu))
			goto mem_err;
		per_cpu(pcpu_pmcs_active, cpu) = false;
		
		if (fixed_pmc_pmi >= 0 && fixed_pmc_pmi < max_pmc_fixed)
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

int recode_pmc_init(void)
{
	int err = 0;
	/* Setup fast IRQ */

	if (recode_pmi_vector == NMI) {
		err = pmi_nmi_setup();
	} else {
		err = pmi_irq_setup();
	}

	if (err) {
		pr_err("Cannot initialize PMI vector\n");
		goto err;
	}

	/* READ MACHINE CONFIGURATION */
	get_machine_configuration();

	if (setup_pmc_on_system(pmc_events_management))
		goto no_cfgs;

	disable_pmc_on_system();

	return err;
no_cfgs:
	if (recode_pmi_vector == NMI) {
		pmi_nmi_cleanup();
	} else {
		pmi_irq_cleanup();
	}
err:
	return -1;	
}

void recode_pmc_fini(void)
{
	/* Disable Recode */
	recode_state = OFF;

	disable_pmc_on_system();

	/* Wait for all PMIs to be completed */
	while (atomic_read(&active_pmis))
		;

	cleanup_pmc_on_system();
	
	if (recode_pmi_vector == NMI) {
		pmi_nmi_cleanup();
	} else {
		pmi_irq_cleanup();
	}
	
	pr_info("PMI uninstalled\n");
}

static void recode_reset_data(void)
{
	unsigned cpu;
	unsigned pmc;

	for_each_online_cpu (cpu) {
		reset_logger(per_cpu(pcpu_pmc_logger, cpu));

		for (pmc = 0; pmc < max_pmc_fixed; ++pmc)
			per_cpu(pcpu_pmcs_snapshot.fixed[pmc], cpu) = 0;

		if (fixed_pmc_pmi >= 0 && fixed_pmc_pmi < max_pmc_fixed)
			per_cpu(pcpu_pmcs_snapshot.fixed[fixed_pmc_pmi], cpu) =
				PMC_TRIM(reset_period);

		for (pmc = 0; pmc < max_pmc_general; ++pmc)
			per_cpu(pcpu_pmcs_snapshot.general[pmc], cpu) = 0;
	}
}

static void ctx_hook(struct task_struct *prev, bool prev_on, bool curr_on)
{
	unsigned cpu = get_cpu();

	switch (recode_state) {
	case OFF:
		if (this_cpu_read(pcpu_pmcs_active))
			disable_pmc_on_cpu();
		break;
	case SYSTEM:
		if (!this_cpu_read(pcpu_pmcs_active))
			enable_pmc_on_cpu();
		break;
	case TUNING:
	case PROFILE:
		/* Toggle PMI */
		if (this_cpu_read(pcpu_pmcs_active) && !curr_on)
			disable_pmc_on_cpu();
		else if (!this_cpu_read(pcpu_pmcs_active) && curr_on)
			enable_pmc_on_cpu();
		break;
	default:
		pr_warn("Invalid state on cpu %u... disabling PMCs\n", cpu);
		disable_pmc_on_cpu();
	}

	// if (cpu == 1) {
	// 	pr_warn("READ_FIXED_PMC(2): %llx\n", READ_FIXED_PMC(2));
	// }

	put_cpu();
}

static void manage_pmu_state(void *dummy)
{
	ctx_hook(NULL, NULL, query_tracker(current->pid));
}

void recode_set_state(unsigned state)
{
	enum recode_state old_state = recode_state;
	recode_state = state;

	if (old_state == recode_state)
		return;

	if (state == OFF) {
		pr_info("Recode state: OFF\n");
		disable_pmc_on_system();
		flush_written_samples_on_system();
		return;
	} if (state == PROFILE) {
		/* Reset DATA and set PROFILE mode */
		pr_info("Recode ready for PROFILE\n");
		recode_reset_data();
	} else if (state == SYSTEM) {
		/* Reset DATA and set SYSTEM mode */
		pr_info("Recode ready for SYSTEM\n");
		recode_reset_data();
	} else {
		pr_warn("Recode invalid state\n");
		recode_state = old_state;
		return;
	}

	/* Use the cached value */
	setup_pmc_on_system(NULL);
	// TODO - Make this call clear
	on_each_cpu(manage_pmu_state, NULL, 0);
}


void pmi_function(unsigned cpu)
{
	bool log = 0;
	u64 msr1, msr2;

	// pr_warn("[%u] PMI\n", cpu);
	/* TODO Fix - query_tracker causes system hang */

	if (atomic_read(&on_samples_flushing))
		return;

	atomic_inc(&active_pmis);

	msr1 = READ_GENERAL_PMC(0);
	
	log = recode_state == SYSTEM;
	if (!log)
		log = (recode_state != TUNING) && query_tracker(current->pid);

	pmc_evaluate_activity(current, log, false);

	msr2 = READ_GENERAL_PMC(0);

	if (msr1 != msr2) {
		pr_debug("** PMCs do not seem Frozen! %llu vs %llu\n", msr1, 
			 msr2);
	}

	atomic_dec(&active_pmis);
}

void pmc_evaluate_activity(struct task_struct *tsk, bool log, bool pmc_off)
{
	unsigned k;
	unsigned cpu = get_cpu();
	pmc_ctr old_fixed_pmi, new_fixed_pmi;
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
	old_fixed_pmi = old_pmcs.fixed[fixed_pmc_pmi];
	new_fixed_pmi = per_cpu(pcpu_pmcs_snapshot.fixed[fixed_pmc_pmi], cpu);

	// TODO - Fix fixed_pmc_pmi = 0

	for_each_pmc(k, max_pmc_fixed + max_pmc_general)
	{
		old_pmcs.pmcs[k] =
			PMC_TRIM(per_cpu(pcpu_pmcs_snapshot.pmcs[k], cpu) -
				 old_pmcs.pmcs[k]);
	}

	/* TSC is not computed and it is set to "this" moment */
	old_pmcs.tsc = per_cpu(pcpu_pmcs_snapshot.tsc, cpu);

	// TODO - Restore in CTX Switch
	// if (old_fixed_pmi >= new_fixed_pmi) {
	// 	old_pmcs.fixed[fixed_pmc_pmi] = 
	// 		PMC_TRIM(((BIT_ULL(48) - 1) - old_fixed_pmi) +
	// 		         new_fixed_pmi);
	// 	this_cpu_add(pcpu_pmcs_snapshot.fixed[fixed_pmc_pmi],
	// 		     reset_period + new_fixed_pmi);
	// } else {
	// 	old_pmcs.fixed[fixed_pmc_pmi] = new_fixed_pmi - old_fixed_pmi;
	// }
	
	if (log)
		write_log_sample(per_cpu(pcpu_pmc_logger, cpu), &old_pmcs);

	if (pmc_off)
		enable_pmc_on_cpu();
		
	put_cpu();
}

/* Register thread for profiling activity  */
int attach_process(pid_t id)
{
	pr_info("Attaching process: %u\n", id);
	tracker_add(id);
	return 0;
}

/* Remove registered thread from profiling activity */
void detach_process(pid_t id)
{
	pr_info("Detaching process: %u\n", id);
	tracker_del(id);
}

int register_ctx_hook(void)
{
	int err = 0;

	set_hook_callback(&ctx_hook);
	switch_hook_set_state_enable(true);
	
	return err;
}

void unregister_ctx_hook(void)
{
	set_hook_callback(NULL);
}
