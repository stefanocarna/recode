/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef _PMU_LOW_H
#define _PMU_LOW_H

/* Intel PMC details */

#define MSR_CORE_PERF_GENERAL_CTR0 MSR_IA32_PERFCTR0
#define MSR_CORE_PERFEVTSEL_ADDRESS0 MSR_P6_EVNTSEL0

#define RDPMC_FIXED_CTR0 BIT(30)
#define RDPMC_GENERAL_CTR0 (0)

#define PERF_GLOBAL_CTRL_FIXED0_SHIFT 32
#define PERF_GLOBAL_CTRL_FIXED0_MASK BIT_ULL(PERF_GLOBAL_CTRL_FIXED0_SHIFT)

#define PMC_TRIM(n) ((n) & (BIT_ULL(48) - 1))
#define PMC_CTR_MAX PMC_TRIM(~0)

#ifdef CONFIG_RUNNING_ON_VM
#define READ_FIXED_PMC(n) n
#define READ_GENERAL_PMC(n) n

#define WRITE_FIXED_PMC(n, v)
#define WRITE_GENERAL_PMC(n, v)
#else
#define READ_FIXED_PMC(n) native_read_pmc(RDPMC_FIXED_CTR0 + n)
#define READ_GENERAL_PMC(n) native_read_pmc(RDPMC_GENERAL_CTR0 + n)

#define WRITE_FIXED_PMC(n, v) wrmsrl(MSR_CORE_PERF_FIXED_CTR0 + n, PMC_TRIM(v))
#define WRITE_GENERAL_PMC(n, v) wrmsrl(MSR_IA32_PMC0 + n, PMC_TRIM(v))
#endif

#define SETUP_GENERAL_PMC(n, v) wrmsrl(MSR_CORE_PERFEVTSEL_ADDRESS0 + n, v)
#define QUERY_GENERAL_PMC(n) native_read_msr(MSR_CORE_PERFEVTSEL_ADDRESS0 + n)

/* Machine info */

#define NR_SYSTEM_PMCS (gbl_nr_pmc_fixed + gbl_nr_pmc_general)

#define FIXED_PMCS_TO_BITS_MASK                                                \
	((BIT_ULL(gbl_nr_pmc_fixed) - 1) << PERF_GLOBAL_CTRL_FIXED0_SHIFT)

#define GENERAL_PMCS_TO_BITS_MASK(nr)                                          \
	(BIT_ULL(nr) - 1)

/* Utility */

#define for_each_pmc(pmc, max) for ((pmc) = 0; (pmc) < (max); ++(pmc))
#define for_each_fixed_pmc(pmc) for_each_pmc (pmc, gbl_nr_pmc_fixed)
#define for_each_general_pmc(pmc) for_each_pmc (pmc, gbl_nr_pmc_general)

#define for_each_active_pmc(ctrl, pmc, off, max)                               \
	for ((pmc) = 0; (pmc) < (max); ++(pmc))                                \
		if (ctrl & BIT_ULL(pmc + off))

#define for_each_active_general_pmc(ctrl, pmc)                                 \
	for_each_active_pmc (ctrl, pmc, 0, gbl_nr_pmc_general)

#define for_each_active_fixed_pmc(ctrl, pmc)                                   \
	for_each_active_pmc (ctrl, pmc, PERF_GLOBAL_CTRL_FIXED0_SHIFT,         \
			     gbl_nr_pmc_fixed)

#define pmcs_fixed(pmcs) (pmcs)
#define pmcs_general(pmcs) (pmcs + gbl_nr_pmc_fixed)
#define nr_pmcs_general(cnt) (cnt - gbl_nr_pmc_fixed)

void get_machine_configuration(void);

#endif /* _PMU_LOW_H */