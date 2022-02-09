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


#define MODNAME	"ReCode"

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

struct group_entity {
	char name[TASK_COMM_LEN];
	uint id;
	void *data;
	spinlock_t lock;
	/* Atomicity is not required */
	uint nr_processes;
	struct list_head p_list;
	bool profiling;
	/* TODO Remove */
	u64 utime;
	u64 stime;
	int nr_active_tasks;
};

struct proc_entity {
	pid_t pid;
	void *data;
	struct task_struct *task;
	struct group_entity *group;
	/* Stats data */
	u64 utime_snap;
	u64 stime_snap;
};

int recode_groups_init(void);
void recode_groups_fini(void);

int register_process_to_group(pid_t pid, struct group_entity *group, void *data);

void *unregister_process_from_group(pid_t pid, struct group_entity *group);

struct group_entity *create_group(char *gname, uint id, void *payload);

struct group_entity *get_group_by_proc(pid_t pid);
struct group_entity *get_group_by_id(uint id);
struct group_entity *get_next_group_by_id(uint id);

void *destroy_group(uint id);

void signal_to_group_by_id(uint signal, uint id);
void signal_to_group(uint signal, struct group_entity *group);
void signal_to_all_groups(uint signal);
void schedule_all_groups(void);
void start_group_stats(struct group_entity *group);
void stop_group_stats(struct group_entity *group);


// void setup_hw_events_from_proc(pmc_evt_code *hw_events_codes, unsigned cnt);


extern void rf_set_state_off(int old_state);
extern void rf_set_state_idle(int old_state);
extern void rf_set_state_profile(int old_state);
extern void rf_set_state_system(int old_state);
extern int rf_set_state_custom(int old_state, int state);
extern void rf_before_set_state(int old_state, int state);

int rf_hook_sched_in_custom_state(struct task_struct *prev,
					 struct task_struct *next);
void rf_after_hook_sched_in(struct task_struct *prev,
				   struct task_struct *next);

/* Pop System Hooks */
extern void pop_hook_sched_in(ARGS_SCHED_IN);
extern void pop_hook_proc_fork(ARGS_PROC_FORK);
extern void pop_hook_proc_exit(ARGS_PROC_EXIT);


int system_hooks_init(void);
void system_hooks_fini(void);

#endif /* _RECODE_CORE_H */
