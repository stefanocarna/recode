#ifndef _RECODE_CORE_H
#define _RECODE_CORE_H

#include <asm/msr.h>
#include <linux/sched.h>
#include <linux/string.h>

#include "recode_structs.h"

#define MODNAME	"ReCode"

#undef pr_fmt
#define pr_fmt(fmt) MODNAME ": " fmt

/*  */
extern u64 perf_global_ctrl;

/*  */
#define MSR_CORE_PERF_GENERAL_CTR0 MSR_IA32_PERFCTR0
#define MSR_CORE_PERFEVTSEL_ADDRESS0 MSR_P6_EVNTSEL0

#define PERF_GLOBAL_CTRL_FIXED0_MASK BIT_ULL(32)
#define PERF_GLOBAL_CTRL_FIXED1_MASK BIT_ULL(33)

#define PMC_TRIM(n) ((n) & (BIT_ULL(48) - 1))

#define READ_FIXED_PMC(n) native_read_msr(MSR_CORE_PERF_FIXED_CTR0 + n)
#define READ_GENERAL_PMC(n) native_read_msr(MSR_CORE_PERF_GENERAL_CTR0 + n)

#define WRITE_FIXED_PMC(n, v) wrmsrl(MSR_CORE_PERF_FIXED_CTR0 + n, PMC_TRIM(v))
#define WRITE_GENERAL_PMC(n, v)                                                \
	wrmsrl(MSR_CORE_PERF_GENERAL_CTR0 + n, PMC_TRIM(v))

#define SETUP_GENERAL_PMC(n, v) wrmsrl(MSR_CORE_PERFEVTSEL_ADDRESS0 + n, v)

typedef unsigned pmc_evt_code;

#define BUFF_SIZE PAGE_SIZE *PAGE_SIZE
#define BUFF_LENGTH ((BUFF_SIZE) / sizeof(struct pmcs_snapshot))

#define RECODE_PMI 239

extern atomic_t active_pmis;

extern atomic_t detected_theads;

extern unsigned __read_mostly fixed_pmc_pmi;
extern unsigned __read_mostly max_pmc_fixed;
extern unsigned __read_mostly max_pmc_general;

extern unsigned long __read_mostly max_pmi_before_ctx;

enum recode_state {
	OFF = 0,
	TUNING = 1,
	PROFILE = 2,
	SYSTEM = 3,
	PT_ONLY = 4,
	IDLE = 5,
};

extern enum recode_state __read_mostly recode_state;

#define for_each_pmc(pmc, max) for ((pmc) = 0; (pmc) < (max); ++(pmc))

#define for_each_fixed_pmc(pmc) for_each_pmc(pmc, max_pmc_fixed)
#define for_each_general_pmc(pmc) for_each_pmc(pmc, max_pmc_general)

extern int recode_data_init(void);
extern void recode_data_fini(void);

extern int recode_pmc_init(void);
extern void recode_pmc_fini(void);

extern void recode_pmc_configure(pmc_evt_code *codes);
extern void recode_set_state(unsigned state);

extern int register_ctx_hook(void);
extern void unregister_ctx_hook(void);

extern int attach_process(pid_t pid);
extern void detach_process(pid_t pid);

/* Statistic Unit */
extern struct pmc_logger *init_logger(unsigned cpu);
extern void fini_logger(struct pmc_logger *logger);
extern void reset_logger(struct pmc_logger *logger);

extern int log_sample(struct pmc_logger *logger, struct pmcs_snapshot *sample);
extern int flush_logs(struct pmc_logger *logger);

extern void process_match(struct task_struct *tsk);

DECLARE_PER_CPU(struct pmc_logger *, pcpu_pmc_logger);

DECLARE_PER_CPU(bool, pcpu_pmcs_active);

DECLARE_PER_CPU(unsigned long, pcpu_pmi_counter);
/* TODO Enable in the future */
// DECLARE_PER_CPU(u64, pcpu_reset_period);

extern pmc_evt_code pmc_events[8]; /* Ignored */
extern pmc_evt_code pmc_events_sc_detection[8];

extern void pmc_evaluate_activity(struct task_struct *tsk, bool log,
				  bool pmc_off);

/* Recode Config */
extern u64 reset_period;

#define NR_THRESHOLDS 5
extern s64 thresholds[NR_THRESHOLDS + 1];

/* Recode PMI */
extern int pmi_recode(void);

/* Recode-internal variables and functions */
extern void enable_pmc_on_cpu(void);
extern void disable_pmc_on_cpu(void);

extern void enable_pmc_on_system(void);
extern void disable_pmc_on_system(void);

extern void get_machine_configuration(void);
extern void read_all_pmcs(struct pmcs_snapshot *snapshot);
extern void setup_pmc_on_system(struct pmc_evt_sel *pmc_cfgs);

extern void enable_pt_on_cpu(void *dummy);
extern void disable_pt_on_cpu(void *dummy);

void decode_pt_buffer(void);

/* Detection metrics */
/* L2_miss / L1_miss */
#define DM0(p, sn) ((sn->general[1] * p) / (sn->general[0] + 1))
/* LLC_miss / L1_miss */
#define DM1(p, sn) ((sn->general[2] * p) / (sn->general[0] + 1))
/* L2_write_back / L2_lines_in */
#define DM2(p, sn) ((sn->general[3] * p) / (sn->general[4] + 1))
/* TLB_l2_miss / L1_miss */
#define DM3(p, sn) ((sn->general[5] * p) / (sn->general[0] + 1))

#define CHECK_LESS_THAN_TS(ts, v, p) ((ts - p) < v)
#define CHECK_MORE_THAN_TS(ts, v, p) (v < (ts + p))

#endif /* _RECODE_CORE_H */