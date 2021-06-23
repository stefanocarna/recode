
#include <asm/ptrace.h>
#include <asm/fast_irq.h>
#include <asm/msr.h>

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
		per_cpu(pcpu_pmcs_snapshot.fixed[1], cpu) =
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
		pmc_cfgs[k].os = 0;
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

	if (free_fast_irq(RECODE_PMI))
		pr_warn("Something wrong while freeing the PMI vector: %u\n", RECODE_PMI);
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

		per_cpu(pcpu_pmcs_snapshot.fixed[1], cpu) =
			PMC_TRIM(reset_period);

		for (pmc = 0; pmc < max_pmc_general; ++pmc)
			per_cpu(pcpu_pmcs_snapshot.general[pmc], cpu) = 0;
	}
}

#define PT_CPU 1

void recode_set_state(unsigned state)
{
	enum recode_state old_state = recode_state;
	recode_state = state;

	if (old_state == recode_state)
		return;

	if (state == OFF && old_state != OFF) {
		disable_pmc_on_system();
		pr_info("Recode state: OFF\n");
		smp_call_function_single(PT_CPU, disable_pt_on_cpu, NULL, 1);
	} else if (state == SYSTEM) {
		/* Reset DATA and set SYSTEM mode */
		recode_reset_data();
		pr_info("Recode ready for SYSTEM\n");
	}  else if (state == IDLE) {
		pr_info("Recode is IDLE\n");
	} else if (state == PT_ONLY) {
		pr_info("Recode is PT_ONLY\n");
		smp_call_function_single(PT_CPU, enable_pt_on_cpu, NULL, 1);
	} else {
		pr_warn("Recode invalid state\n");
		recode_state = old_state;
		return;
	}

	setup_pmc_on_system(pmc_cfgs);
}

static void ctx_hook(struct task_struct *prev, bool prev_on, bool curr_on)
{
	if (recode_state == SYSTEM || recode_state == IDLE) {
		/* Just enable PMCs if they are disabled */
		if (!this_cpu_read(pcpu_pmcs_active))
			enable_pmc_on_cpu();

		// TODO do something
	} else {
		/* Skip pmc-off CPUs and Kernel Threads */

		/* Toggle PMI */
		if (this_cpu_read(pcpu_pmcs_active) && !curr_on)
			disable_pmc_on_cpu();

		else if (!this_cpu_read(pcpu_pmcs_active) && curr_on)
			enable_pmc_on_cpu();
	}
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
