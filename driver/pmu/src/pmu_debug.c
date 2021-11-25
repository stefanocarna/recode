#include "pmu_low.h"
#include "pmu.h"

void debug_pmu_state(void)
{
	u64 msr;
	uint pmc;
	uint cpu = get_cpu();

	pr_debug("Init PMU debug on core %u\n", cpu);

	rdmsrl(MSR_CORE_PERF_GLOBAL_STATUS, msr);
	pr_debug("MSR_CORE_PERF_GLOBAL_STATUS: %llx\n", msr);

	rdmsrl(MSR_CORE_PERF_FIXED_CTR_CTRL, msr);
	pr_debug("MSR_CORE_PERF_FIXED_CTR_CTRL: %llx\n", msr);

	rdmsrl(MSR_CORE_PERF_GLOBAL_CTRL, msr);
	pr_debug("MSR_CORE_PERF_GLOBAL_CTRL: %llx\n", msr);

	pr_debug("GP_sel 0: %llx\n", QUERY_GENERAL_PMC(0));
	pr_debug("GP_sel 1: %llx\n", QUERY_GENERAL_PMC(1));
	pr_debug("GP_sel 2: %llx\n", QUERY_GENERAL_PMC(2));
	pr_debug("GP_sel 3: %llx\n", QUERY_GENERAL_PMC(3));

	for_each_fixed_pmc (pmc) {
		pr_debug("FX_ctrl %u: %llx\n", pmc, READ_FIXED_PMC(pmc));
	}

	for_each_general_pmc (pmc) {
		pr_debug("GP_ctrl %u: %llx\n", pmc, READ_GENERAL_PMC(pmc));
	}

	pr_debug("Fini PMU debug on core %u\n", cpu);

	put_cpu();
}
EXPORT_SYMBOL(debug_pmu_state);

void reset_pmc_local(void *dummy)
{
	uint pmc;

	for_each_fixed_pmc(pmc)
		WRITE_FIXED_PMC(pmc, 0ULL);

	for_each_general_pmc(pmc)
		WRITE_GENERAL_PMC(pmc, 0ULL);
}

void reset_pmc_global(void)
{
	on_each_cpu(reset_pmc_local, NULL, 1);
}
