/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef _RECODE_CORE_H
#define _RECODE_CORE_H

#include <asm/bug.h>
#include <asm/msr.h>
#include <linux/sched.h>
#include <linux/string.h>

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
	TUNING = 1,
	PROFILE = 2,
	SYSTEM = 3,
	IDLE = 4, // Useless
	KILL = 5
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
extern int wrecode_data_init(void);
extern void recode_data_fini(void);

extern int recode_pmc_init(void);
extern void recode_pmc_fini(void);

extern void recode_set_state(unsigned state);

extern int attach_process(struct task_struct *tsk);
extern void detach_process(struct task_struct *tsk);

/* Groups */
extern uint nr_groups;

struct group_entity {
	uint id;
	void *data;
	spinlock_t lock;
	/* Atomicity is not required */
	uint nr_processes;
	struct list_head p_list;
};

struct proc_entity {
	pid_t pid;
	void *data;
	struct task_struct *task;
	struct group_entity *group;
};

int recode_groups_init(void);
void recode_groups_fini(void);

int register_process_to_group(pid_t pid, struct group_entity *group, void *data);

void *unregister_process_from_group(pid_t pid, struct group_entity *group);

struct group_entity *create_group(uint id, void *payload);

struct group_entity *get_group_by_proc(pid_t pid);
struct group_entity *get_group_by_id(uint id);
struct group_entity *get_next_group_by_id(uint id);

void *destroy_group(uint id);

void signal_to_group(uint signal, uint id);
void signal_to_all_groups(uint signal);
void schedule_all_groups(void);


// void setup_hw_events_from_proc(pmc_evt_code *hw_events_codes, unsigned cnt);

#endif /* _RECODE_CORE_H */
