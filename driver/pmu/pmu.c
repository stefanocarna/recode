#include <asm/apic.h>
#include <asm/perf_event.h>
#include <linux/sched.h>
#include <linux/slab.h>

#include "recode.h"
#include "pmu/pmu.h"
#include "pmu/pmi.h"
#include "recode_config.h"

// struct pmc_multiplexer {
// 	unsigned cnt;
// 	unsigned max;
// 	pmc_evt_sel *cfgs;
// };

u64 perf_global_ctrl = 0xFULL | BIT_ULL(32) | BIT_ULL(33) | BIT_ULL(34);
u64 fixed_ctrl = 0;
// u64 fixed_ctrl = 0x3B3; // Enable OS + USR
// u64 fixed_ctrl = 0x3A3; // Enable USR only
// u64 fixed_ctrl = 0x393; // Enable OS only

unsigned __read_mostly nr_pmc_fixed = 3;
unsigned __read_mostly nr_pmc_general = 4;

DEFINE_PER_CPU(bool ,pcpu_pmcs_active) = false;

pmc_evt_code *pmc_events;

/* Internal PMC configuration cache */
struct pmc_evt_sel *pmc_cfgs = NULL;


void cleanup_pmc_on_system(void)
{
	kfree(pmc_cfgs);
}

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
	nr_pmc_general = eax.split.num_counters;
	perf_global_ctrl = (BIT(nr_pmc_general) - 1) | 
	                    BIT(32) | BIT(33) |BIT(34);

	pr_info("PMU Version: %u\n", version);
	pr_info(" - NR Counters: %u\n", eax.split.num_counters);
	pr_info(" - Counter's Bits: %u\n", eax.split.bit_width);
	pr_info(" - Counter's Mask: %llx\n", (1ULL << eax.split.bit_width) - 1);
	pr_info(" - NR PEBS' events: %x\n",
		min_t(unsigned, 8, eax.split.num_counters));
}

void fast_setup_general_pmc_on_cpu(void *pmc_cfgs)
{
	unsigned pmc;
	struct pmc_evt_sel *cfgs = (struct pmc_evt_sel *) pmc_cfgs;

	for_each_general_pmc(pmc) {
		SETUP_GENERAL_PMC(pmc, cfgs[pmc].perf_evt_sel);
		WRITE_GENERAL_PMC(pmc, 0ULL);
	}
}

static void __setup_pmc_on_cpu(void *pmc_cfgs)
{
#ifndef CONFIG_RUNNING_ON_VM
	u64 msr;
	unsigned pmc;
	struct pmc_evt_sel *cfgs = (struct pmc_evt_sel *) pmc_cfgs;

	if (!cfgs) {
		pr_warn("Cannot setup PMCs with a NULL conf\n");
		return;
	}

	/* Refresh APIC entry */
	if (recode_pmi_vector == NMI)
		apic_write(APIC_LVTPC, LVT_NMI);
	else
		apic_write(APIC_LVTPC, RECODE_PMI);

	/* Clear a possible stale state */
	rdmsrl(MSR_CORE_PERF_GLOBAL_STATUS, msr);
	wrmsrl(MSR_CORE_PERF_GLOBAL_OVF_CTRL, msr);

	/* Enable FREEZE_ON_PMI */
	wrmsrl(MSR_IA32_DEBUGCTLMSR, BIT(12));

	fast_setup_general_pmc_on_cpu(cfgs);

	for_each_fixed_pmc(pmc) {
		/**
		 * bits: 3   2   1   0
		 * 	PMI, 0, USR, OS
		 */
		if (pmc == fixed_pmc_pmi) {
			WRITE_FIXED_PMC(pmc, reset_period);
			/* Set PMI */
			fixed_ctrl |= (BIT(3) << (pmc * 4));
			if (params_cpl_usr)
				fixed_ctrl |= (BIT(1) << (pmc * 4));
			if (params_cpl_os)
				fixed_ctrl |= (BIT(0) << (pmc * 4));
		} else {
			WRITE_FIXED_PMC(pmc, 0ULL);
			fixed_ctrl |= ((BIT(0) | BIT(1)) << (pmc * 4)); /* 0011 -> USR, OS */
		}
	}

	/* Setup FIXED PMCs */
	wrmsrl(MSR_CORE_PERF_FIXED_CTR_CTRL, fixed_ctrl);

	// debug_pmu_state();
#else
	pr_warn("CONFIG_RUNNING_ON_VM is enabled. PMCs are ignored\n");
#endif
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
	for_each_fixed_pmc(pmc) {
		if (perf_global_ctrl & BIT_ULL(pmc)) {
			snapshot->fixed[pmc] = READ_FIXED_PMC(pmc);
		}
	}
	/* Read all active general pmcs */
	for_each_general_pmc(pmc) {
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
#ifndef CONFIG_RUNNING_ON_VM
	wrmsrl(MSR_CORE_PERF_GLOBAL_CTRL, perf_global_ctrl);
#endif
exit:
	put_cpu();
}

static void __disable_pmc_on_cpu(void *dummy)
{
	per_cpu(pcpu_pmcs_active, get_cpu()) = false;
#ifndef CONFIG_RUNNING_ON_VM
	wrmsrl(MSR_CORE_PERF_GLOBAL_CTRL, 0ULL);
#endif
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

void debug_pmu_state(void)
{
	u64 msr;
	unsigned cpu = get_cpu();

	pr_info("Init PMU debug on core %u\n", cpu);

	rdmsrl(MSR_CORE_PERF_GLOBAL_STATUS, msr);
	pr_info("MSR_CORE_PERF_GLOBAL_STATUS: %llx\n", msr);

	rdmsrl(MSR_CORE_PERF_FIXED_CTR_CTRL, msr);
	pr_info("MSR_CORE_PERF_FIXED_CTR_CTRL: %llx\n", msr);

	pr_info("GP 0: %llx\n", QUERY_GENERAL_PMC(0));
	pr_info("GP 1: %llx\n", QUERY_GENERAL_PMC(1));
	pr_info("GP 2: %llx\n", QUERY_GENERAL_PMC(2));
	pr_info("GP 3: %llx\n", QUERY_GENERAL_PMC(3));

	pr_info("Fini PMU debug on core %u\n", cpu);

	put_cpu();
}


int setup_pmc_on_system(pmc_evt_code *codes)
{
	unsigned pmc;

	if (!pmc_cfgs) {
		pmc_cfgs = kzalloc(sizeof(struct pmc_evt_sel) * nr_pmc_general,
				   GFP_KERNEL);
		if (!pmc_cfgs) {
			pr_err("Cannot allocate memory for PMC configs\n");
			return -ENOMEM;
		}
	}

	if (!codes)
		goto old_conf;
		
	// TODO - compact setup
	// TODO - Check memory leaks
	pmc_events = codes;

	for_each_general_pmc(pmc) {
		pmc_cfgs[pmc].perf_evt_sel = codes[pmc];

		/* PMCs setup */
		pmc_cfgs[pmc].usr = !!(params_cpl_usr);
		pmc_cfgs[pmc].os = !!(params_cpl_os);
		pmc_cfgs[pmc].pmi = 0;
		pmc_cfgs[pmc].en = 1;
	}

old_conf:
	on_each_cpu(__setup_pmc_on_cpu, pmc_cfgs, 1);
	pr_warn("PMCs setup on all cores\n");

	return 0;
}