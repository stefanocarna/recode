/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _HOOKS_H
#define _HOOKS_H

#include <linux/binfmts.h>

enum hook_type {
	SCHED_IN,
	SCHED_OUT,
	PROC_EXEC,
	PROC_EXIT,
	PROC_FORK,
	PROC_FREE
};

// 	char *name;
// 	void (*switch_in)(void *data, bool preempt, struct task_struct *prev,
// 				struct task_struct *next),
// 	void (*switch_out)(struct task_struct *prev, struct task_struct *next),
// 	void (*proc_exec)(void *data, struct task_struct *p,
// 				     pid_t old_pid, struct linux_binprm *bprm),
// 	void (*proc_fork)(void *data, struct task_struct *parent, struct task_struct *child),
// 	void (*proc_free)(void *data, struct task_struct *p),
// 	void (*proc_exit)(void *data, struct task_struct *p)
// };

#define ARGS_DATA(args...) void *data, args
#define ARGS_SCHED_IN                                                          \
	ARGS_DATA(bool preempt, struct task_struct *prev,                      \
		  struct task_struct *next)
#define ARGS_PROC_FORK                                                         \
	ARGS_DATA(struct task_struct *parent, struct task_struct *child)
#define ARGS_PROC_EXIT ARGS_DATA(struct task_struct *p)

int register_hook(enum hook_type type, void *func);
void unregister_hook(enum hook_type type, void *func);

#endif /* _HOOKS_H */