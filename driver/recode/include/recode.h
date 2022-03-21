/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef _RECODE_CORE_H
#define _RECODE_CORE_H

#include <asm/bug.h>
#include <asm/msr.h>
#include <linux/sched.h>
#include <linux/string.h>

#include "pmu_structs.h"
#include "hooks.h"

#if __has_include(<asm/fast_irq.h>)
#define FAST_IRQ_ENABLED 1
#endif
#if __has_include(<asm/fast_irq.h>) && defined(SECURITY_MODULE)
#define SECURITY_MODULE_ON 1
#elif defined(TMA_MODULE)
#define TMA_MODULE_ON 1
#endif

#define MODNAME "ReCode"

#undef pr_fmt
#define pr_fmt(fmt) MODNAME ": " fmt

enum recode_state {
	OFF = 0,
	PROFILE = 2,
	SYSTEM = 3,
	IDLE = 4, // Useless
};

extern enum recode_state __read_mostly recode_state;

struct recode_callbacks {
	// void (*on_hw_events_change)(struct hw_events *events);
	// void (*on_pmi)(unsigned cpu, struct pmus_metadata *pmus_metadata);
	void (*on_ctx)(struct task_struct *prev, struct task_struct *next);
	bool (*on_state_change)(enum recode_state);
};

extern struct recode_callbacks __read_mostly recode_callbacks;

/* Recode module */
extern int recode_data_init(void);
extern void recode_data_fini(void);

extern int recode_pmc_init(void);
extern void recode_pmc_fini(void);

extern void recode_set_state(int state);

extern int attach_process(struct task_struct *tsk, char *gname);
extern void detach_process(struct task_struct *tsk);

void rf_on_pmi_callback(uint cpu, struct pmus_metadata *pmus_metadata);

int track_thread(struct task_struct *task);
bool query_tracked(struct task_struct *task);
void untrack_thread(struct task_struct *task);

/* Groups */
extern uint nr_groups;

struct process_stats {

	bool alive;

	// CPU time
	unsigned long cpu_time;

	// Struct to collect pmcs data
	int nr_samples;
	struct stats_sample *samples_head;
	struct stats_sample *samples_tail;
};


struct group_stats {

	// CPU time - Sum of the processes' time
	unsigned long cpu_time;

	// Power Comnsumption
	unsigned long power;

	// State - True if at least one process is alive
	bool alive;

	// Thread stats
	int nr_processes;
	struct process_stats process_stats_list;
};


// void setup_hw_events_from_proc(pmc_evt_code *hw_events_codes, unsigned cnt);

extern void rf_set_state_off(int old_state);
extern void rf_set_state_idle(int old_state);
extern void rf_set_state_profile(int old_state);
extern void rf_set_state_system(int old_state);
extern int rf_set_state_custom(int old_state, int state);
extern void rf_before_set_state(int old_state, int state);

int rf_hook_sched_in_custom_state(struct task_struct *prev,
				  struct task_struct *next);
void rf_after_hook_sched_in(struct task_struct *prev, struct task_struct *next);

/* Pop System Hooks */
extern void pop_hook_sched_in(ARGS_SCHED_IN);
extern void pop_hook_proc_fork(ARGS_PROC_FORK);
extern void pop_hook_proc_exit(ARGS_PROC_EXIT);

int system_hooks_init(void);
void system_hooks_fini(void);

#endif /* _RECODE_CORE_H */
