#include <linux/sched.h>
#include <asm/perf_event.h>

#include "recode.h"

u64 perf_global_ctrl = 0xFULL | BIT_ULL(32) | BIT_ULL(33) | BIT_ULL(34);
u64 fixed_ctrl = 0x3B3;

unsigned __read_mostly max_pmc_fixed = 3;
unsigned __read_mostly max_pmc_general = 4;

DEFINE_PER_CPU(bool ,pcpu_pmcs_active) = false;

void get_machine_configuration(void)
{
	union cpuid10_edx edx;
	union cpuid10_eax eax;
	union cpuid10_ebx ebx;
	unsigned int unused;
	unsigned version;

	cpuid(10, &eax.full, &ebx.full, &unused, &edx.full);

	if (eax.split.mask_length < 7)
		return;

	version = eax.split.version_id;

	pr_info("RECODE gets PMU CONF:\n");
	pr_info("Version: %u\n", version);
	pr_info("Counters: %u\n", eax.split.num_counters);
	pr_info("Counter's Bits: %u\n", eax.split.bit_width);
	pr_info("Counter's Mask: %llx\n", (1ULL << eax.split.bit_width) - 1);

	pr_info("Evt's Bits: %u\n", ebx.full);
	pr_info("Evt's Mask: %x\n", eax.split.mask_length);

	pr_info("PEBS MAX EVTs: %x\n",
		min_t(unsigned, 8, eax.split.num_counters));

	max_pmc_general = eax.split.num_counters;
	perf_global_ctrl = (BIT(max_pmc_general) - 1) | BIT(32) | BIT(33) | BIT(34);
}

static void __setup_pmc_on_cpu(void *pmc_cfgs)
{
	u64 msr;
	unsigned k;
	struct pmc_evt_sel *cfgs = (struct pmc_evt_sel *) pmc_cfgs;

	if (!cfgs) {
		pr_warn("Cannot setup PMCs with a NULL conf\n");
		return;
	}

	/* Refresh APIC entry */
	apic_write(APIC_LVTPC, RECODE_PMI);

	/* Clear a possible stale state */
	rdmsrl(MSR_CORE_PERF_GLOBAL_STATUS, msr);
	wrmsrl(MSR_CORE_PERF_GLOBAL_OVF_CTRL, msr);

	/* Setup FIXED PMCs */
	wrmsrl(MSR_CORE_PERF_FIXED_CTR_CTRL, fixed_ctrl);

	/* Enable FREEZE_ON_PMI */
	wrmsrl(MSR_IA32_DEBUGCTLMSR, BIT(12));

	for (k = 0; k < max_pmc_general; ++k) {
		SETUP_GENERAL_PMC(k, cfgs[k].perf_evt_sel);
		WRITE_GENERAL_PMC(k, 0ULL);
	}

	WRITE_FIXED_PMC(0, 0ULL);
	WRITE_FIXED_PMC(1, reset_period);
	WRITE_FIXED_PMC(2, 0ULL);
}

void setup_pmc_on_system(struct pmc_evt_sel *pmc_cfgs)
{
	on_each_cpu(__setup_pmc_on_cpu, pmc_cfgs, 1);
}

void read_all_pmcs(struct pmcs_snapshot *snapshot)
{
	unsigned pmc;
	if (!snapshot) {
		pr_warn("Cannot save PMCs on NULL snapshot\n");
		return;
	}

	snapshot->tsc = (u64)rdtsc_ordered();

	/* Read all active fixed pmcs */
	for_each_fixed_pmc(pmc)
	{
		if (perf_global_ctrl & BIT_ULL(pmc)) {
			snapshot->fixed[pmc] = READ_FIXED_PMC(pmc);
		}
	}
	/* Read all active general pmcs */
	for_each_general_pmc(pmc)
	{
		if (perf_global_ctrl & BIT_ULL(pmc)) {
			snapshot->general[pmc] = READ_GENERAL_PMC(pmc);
		}
	}
}

static void __enable_pmc_on_cpu(void *dummy)
{
	unsigned cpu = get_cpu();
	if (recode_state == OFF) {
		pr_warn("Cannot enable pmc on cpu %u while Recode is OFF\n",
			cpu);
		goto exit;
	}
	per_cpu(pcpu_pmcs_active, cpu) = true;
	wrmsrl(MSR_CORE_PERF_GLOBAL_CTRL, perf_global_ctrl);
exit:
	put_cpu();
}

static void __disable_pmc_on_cpu(void *dummy)
{
	per_cpu(pcpu_pmcs_active, get_cpu()) = false;
	wrmsrl(MSR_CORE_PERF_GLOBAL_CTRL, 0ULL);
	put_cpu();
}

void inline __attribute__((always_inline)) enable_pmc_on_cpu(void)
{
	__enable_pmc_on_cpu(NULL);
}

void inline __attribute__((always_inline)) disable_pmc_on_cpu(void)
{
	__disable_pmc_on_cpu(NULL);
}

void inline __attribute__((always_inline)) enable_pmc_on_system(void)
{
	on_each_cpu(__enable_pmc_on_cpu, NULL, 1);
}

void inline __attribute__((always_inline)) disable_pmc_on_system(void)
{
	on_each_cpu(__disable_pmc_on_cpu, NULL, 1);
}