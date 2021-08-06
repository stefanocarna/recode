#ifndef _RECODE_CORE_H
#define _RECODE_CORE_H

#include <asm/bug.h>
#include <asm/msr.h>
#include <linux/sched.h>
#include <linux/string.h>

#include "pmu/pmu_structs.h"

#if __has_include(<asm/fast_irq.h>)
#define FAST_IRQ_ENABLED 1
#endif

#define MODNAME	"ReCode"

#undef pr_fmt
#define pr_fmt(fmt) MODNAME ": " fmt

enum recode_state {
	OFF = 0,
	TUNING = 1,
	PROFILE = 2,
	SYSTEM = 3,
	IDLE = 4, // Useless
};

extern enum recode_state __read_mostly recode_state;


/* Recode module */
extern int recode_data_init(void);
extern void recode_data_fini(void);

extern int recode_pmc_init(void);
extern void recode_pmc_fini(void);

extern void recode_set_state(unsigned state);

extern int register_ctx_hook(void);
extern void unregister_ctx_hook(void);

extern int attach_process(pid_t id);
extern void detach_process(pid_t id);

/* Recode PMI */
extern void pmi_function(unsigned cpu);

void setup_hw_events_from_proc(pmc_evt_code *hw_events_codes, unsigned cnt);

/* Recode TMA */
// extern void pmc_evaluate_tma(unsigned cpu, struct pmcs_snapshot *pmcs);

#endif /* _RECODE_CORE_H */