/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef _PMU_CORE_H
#define _PMU_CORE_H

int system_hooks_init(void);

void system_hooks_fini(void);

bool create_task_data(struct task_struct *task);

void destroy_task_data(struct task_struct *task);

struct pmu_tasks_data *get_task_data(struct task_struct *task);

bool exist_or_create_task_data(struct task_struct *task);


/* TODO CHNAGE */

extern bool use_pid_register_id;

#define TRACKER_GET_ID(tsk) ((use_pid_register_id) ? tsk->pid : tsk->tgid)

extern int tracker_add(struct task_struct *tsk);
extern int tracker_del(struct task_struct *tsk);
extern bool query_tracker(struct task_struct *tsk);

extern int tracker_init(void);
extern void tracker_fini(void);

extern void set_exit_callback(smp_call_func_t callback);

struct tp_node {
	pid_t id;
	unsigned counter;
	struct hlist_node node;
};

#endif /* _PMU_CORE_H */
