// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note

#include <asm/apic.h>
#include <asm/perf_event.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/cpumask.h>

#include "pmu_low.h"
#include "pmu.h"
#include "pmi.h"
#include "hw_events.h"
#include "pmu_config.h"
#include "logic/tma.h"

// TODO Refactor
DEFINE_PER_CPU(struct pmus_metadata, pcpu_pmus_metadata) = { 0 };
EXPORT_PER_CPU_SYMBOL(pcpu_pmus_metadata);

unsigned __read_mostly gbl_nr_pmc_fixed;
unsigned __read_mostly gbl_nr_pmc_general;

static struct cpumask __read_mostly pmu_enabled_cpumask;

bool __read_mostly pmu_enabled;

static pmc_ctr *__hw_pmcs;

void get_machine_configuration(void)
{
	union cpuid10_edx edx;
	union cpuid10_eax eax;
	union cpuid10_ebx ebx;
	uint unused;
	uint version;

	cpuid(10, &eax.full, &ebx.full, &unused, &edx.full);

	if (eax.split.mask_length < 7)
		return;

	version = eax.split.version_id;
	gbl_nr_pmc_general = eax.split.num_counters;
	gbl_nr_pmc_fixed = edx.split.num_counters_fixed;

	pr_info("PMU Version: %u\n", version);
	pr_info(" - NR Counters: %u\n", eax.split.num_counters);
	pr_info(" - Counter's Bits: %u\n", eax.split.bit_width);
	pr_info(" - Counter's Mask: %llx\n", (1ULL << eax.split.bit_width) - 1);
	pr_info(" - NR PEBS' events: %x\n",
		min_t(uint, 8, eax.split.num_counters));
}

void pmudrv_set_state(bool state)
{
	if (state == pmu_enabled)
		return;

	pr_info("State set to %s\n", state ? "ON" : "OFF");
	pmu_enabled = state;

	if (pmu_enabled)
		enable_pmcs_global();
	else
		disable_pmcs_global();
}
EXPORT_SYMBOL(pmudrv_set_state);

static void __enable_pmcs_local(void *dummy)
{
	u64 a;
	uint cpu = smp_processor_id();

	if (!pmu_enabled || !cpumask_test_cpu(cpu, &pmu_enabled_cpumask)) {
		// pr_warn("Cannot enable pmu on cpu %u. State is OFF\n", cpu);
		return;
	}
	this_cpu_write(pcpu_pmus_metadata.pmcs_active, true);
	// pr_debug("enabled pmcs on cpu %u - gbl_ctrl: %llx\n", cpu,
	// 	 this_cpu_read(pcpu_pmus_metadata.perf_global_ctrl));
#ifndef CONFIG_RUNNING_ON_VM
	wrmsrl(MSR_CORE_PERF_GLOBAL_CTRL,
	       this_cpu_read(pcpu_pmus_metadata.perf_global_ctrl));
	       rdmsrl(MSR_CORE_PERF_GLOBAL_CTRL, a);
	pr_info("-- %llx\n", this_cpu_read(pcpu_pmus_metadata.perf_global_ctrl));
	pr_info("%llx\n", a);
#endif
}

static void __disable_pmcs_local(void *dummy)
{
	this_cpu_write(pcpu_pmus_metadata.pmcs_active, false);
	// pr_debug("disabled pmcs on cpu %u\n", smp_processor_id());
#ifndef CONFIG_RUNNING_ON_VM
	wrmsrl(MSR_CORE_PERF_GLOBAL_CTRL, 0ULL);
#endif
}

void enable_pmcs_local(bool force)
{
	if (!this_cpu_read(pcpu_pmus_metadata.pmcs_active))
		__enable_pmcs_local(NULL);
}
EXPORT_SYMBOL(enable_pmcs_local);

void disable_pmcs_local(bool force)
{
	if (this_cpu_read(pcpu_pmus_metadata.pmcs_active))
		__disable_pmcs_local(NULL);
}
EXPORT_SYMBOL(disable_pmcs_local);

// TODO Safety
void restore_and_enable_pmcs_local(bool *state)
{
	if (*state)
		enable_pmcs_local(true);
}

// TODO Safety
void save_and_disable_pmcs_local(bool *state)
{
	*state = this_cpu_read(pcpu_pmus_metadata.pmcs_active);

	if (*state)
		disable_pmcs_local(true);
}

void enable_pmcs_global(void)
{
	on_each_cpu(__enable_pmcs_local, NULL, 1);
}
EXPORT_SYMBOL(enable_pmcs_global);

void disable_pmcs_global(void)
{
	on_each_cpu(__disable_pmcs_local, NULL, 1);
}
EXPORT_SYMBOL(disable_pmcs_global);

static void __init_pmu_local(void *hw_pmcs_p)
{
#ifndef CONFIG_RUNNING_ON_VM
	u64 msr;
	uint pmc, offset;
	pmc_ctr *hw_pmcs = (pmc_ctr *)hw_pmcs_p;

	__disable_pmcs_local(NULL);

	/* Refresh APIC entry */
	if (pmi_vector == NMI)
		apic_write(APIC_LVTPC, LVT_NMI);
	else
		apic_write(APIC_LVTPC, RECODE_PMI);

	/* Clear a possible stale state */
	rdmsrl(MSR_CORE_PERF_GLOBAL_STATUS, msr);
	wrmsrl(MSR_CORE_PERF_GLOBAL_OVF_CTRL, msr);

	/* Enable FREEZE_ON_PMI */
	wrmsrl(MSR_IA32_DEBUGCTLMSR, BIT(12));

	for_each_fixed_pmc(pmc) {
		if (pmc == gbl_fixed_pmc_pmi) {
			/* TODO Check */
			WRITE_FIXED_PMC(pmc, PMC_TRIM(~gbl_reset_period));
		} else {
			WRITE_FIXED_PMC(pmc, 0ULL);
		}
	}

	/* Setup FIXED PMCs */
	wrmsrl(MSR_CORE_PERF_FIXED_CTR_CTRL,
	       this_cpu_read(pcpu_pmus_metadata.fixed_ctrl));

	/* Assign the memory for the fixed PMCs snapshot */
	offset = smp_processor_id() * NR_SYSTEM_PMCS;
	this_cpu_write(pcpu_pmus_metadata.hw_pmcs, hw_pmcs + offset);

	/* Assign here the memory for the per-cpu pmc-collection */
	this_cpu_write(
		pcpu_pmus_metadata.pmcs_collection,
		kzalloc(sizeof(struct pmcs_collection) +
				(sizeof(pmc_ctr) * MAX_PARALLEL_HW_EVENTS),
			GFP_KERNEL));

#else
	pr_warn("CONFIG_RUNNING_ON_VM is enabled. PMCs are ignored\n");
#endif
}

int pmu_global_init(void)
{
	int err, cpu, pmc;
	u64 gbl_fixed_ctrl = 0;
	/* Compute fixed_ctrl */

	/* TODO Make modular */
	cpumask_copy(&pmu_enabled_cpumask, cpu_all_mask);
	// cpumask_clear(&pmu_enabled_cpumask);
	// pr_warn("*** PMUDRV set only on CPU 2 ***\n");
	// cpumask_set_cpu(2, &pmu_enabled_cpumask);

	pr_debug("num_possible_cpus: %u\n", num_possible_cpus());

	__hw_pmcs = kvcalloc(sizeof(pmc_ctr),
			     num_possible_cpus() * NR_SYSTEM_PMCS, GFP_KERNEL);
	if (!__hw_pmcs) {
		err = -ENOMEM;
		goto no_mem;
	}

	for_each_fixed_pmc(pmc) {
		/**
		 * bits: 3   2   1   0
		 * 	PMI, 0, USR, OS
		 */
		if (pmc == gbl_fixed_pmc_pmi) {
			/* Set PMI */
			gbl_fixed_ctrl |= (BIT(3) << (pmc * 4));
		}
		if (params_cpl_usr)
			gbl_fixed_ctrl |= (BIT(1) << (pmc * 4));
		if (params_cpl_os)
			gbl_fixed_ctrl |= (BIT(0) << (pmc * 4));
	}

	for_each_online_cpu(cpu) {
		per_cpu(pcpu_pmus_metadata.fixed_ctrl, cpu) = gbl_fixed_ctrl;
	}

	/* Metadata doesn't require initialization at the moment */
	on_each_cpu(__init_pmu_local, __hw_pmcs, 1);

	err = tma_init();
	if (err)
		goto no_tma;

	pr_info("PMI set on fixed pmc %u\n", gbl_fixed_pmc_pmi);
	pr_warn("PMUs initialized on all cores\n");
	pr_warn("HW Events: %u\n", NR_HW_EVENTS);

	return 0;

no_tma:
	kvfree(__hw_pmcs);
no_mem:
	return err;
}

void pmu_global_fini(void)
{
	/* This cal already disables pmu globally */
	tma_fini();

	/* This is hw_pmcs allocated in init_pmu_global */
	kvfree(__hw_pmcs);
}