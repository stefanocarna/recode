#ifndef _RECODE_PMU_H
#define _RECODE_PMU_H

#define MSR_CORE_PERF_GENERAL_CTR0 MSR_IA32_PERFCTR0
#define MSR_CORE_PERFEVTSEL_ADDRESS0 MSR_P6_EVNTSEL0

#define PERF_GLOBAL_CTRL_FIXED0_MASK BIT_ULL(32)
#define PERF_GLOBAL_CTRL_FIXED1_MASK BIT_ULL(33)

#define PMC_TRIM(n) ((n) & (BIT_ULL(48) - 1))

#ifdef CONFIG_RUNNING_ON_VM
#define READ_FIXED_PMC(n) n
#define READ_GENERAL_PMC(n) n

#define WRITE_FIXED_PMC(n, v)
#define WRITE_GENERAL_PMC(n, v)
#else
#define READ_FIXED_PMC(n) native_read_msr(MSR_CORE_PERF_FIXED_CTR0 + n)
#define READ_GENERAL_PMC(n) native_read_msr(MSR_CORE_PERF_GENERAL_CTR0 + n)

#define WRITE_FIXED_PMC(n, v) wrmsrl(MSR_CORE_PERF_FIXED_CTR0 + n, PMC_TRIM(v))
#define WRITE_GENERAL_PMC(n, v)                                                \
	wrmsrl(MSR_CORE_PERF_GENERAL_CTR0 + n, PMC_TRIM(v))
#endif

#define SETUP_GENERAL_PMC(n, v) wrmsrl(MSR_CORE_PERFEVTSEL_ADDRESS0 + n, v)
#define QUERY_GENERAL_PMC(n) native_read_msr(MSR_CORE_PERFEVTSEL_ADDRESS0 + n)

typedef u64 pmc_evt_code;

extern u64 perf_global_ctrl;

extern pmc_evt_code *pmc_events;

extern unsigned __read_mostly max_pmc_fixed;
extern unsigned __read_mostly max_pmc_general;

void enable_pmc_on_cpu(void);
void disable_pmc_on_cpu(void);

void enable_pmc_on_system(void);
void disable_pmc_on_system(void);

void get_machine_configuration(void);
void read_all_pmcs(struct pmcs_snapshot *snapshot);

int setup_pmc_on_system(pmc_evt_code *pmc_cfgs);
void cleanup_pmc_on_system(void);

/* Debug functions */
void debug_pmu_state(void);

#endif /* _RECODE_PMU_H */