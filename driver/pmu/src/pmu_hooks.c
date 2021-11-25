// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note

#include <linux/perf_event.h>

#include "pmu.h"
#include "pmu_core.h"

#include "hooks.h"

static void pmu_hook_proc_fork(void *data, struct task_struct *parent,
			       struct task_struct *child)
{
	if (!pmu_enabled)
		return;

	preempt_enable_notrace();

	if (get_task_data(parent))
		create_task_data(child);

	preempt_disable_notrace();
}

static void pmu_hook_sched_in(void *data, bool preempt,
			      struct task_struct *prev,
			      struct task_struct *next)
{
	if (!pmu_enabled)
		return;

	// reset_hw_events_local();
}

static void pmu_hook_proc_exit(void *data, struct task_struct *p)
{
	destroy_task_data(p);
}

struct hook_func {
	void *func;
	enum hook_type type;
};

static struct hook_func hook_funcs[] = { { pmu_hook_sched_in, SCHED_IN },
					 { pmu_hook_proc_fork, PROC_FORK },
					 { pmu_hook_proc_exit, PROC_EXIT } };

int system_hooks_init(void)
{
	uint i;

	for (i = 0; i < ARRAY_SIZE(hook_funcs); ++i) {
		if (register_hook(hook_funcs[i].type, hook_funcs[i].func))
			goto no_hooks;
	}

	return 0;

no_hooks:
	for (i = i - 1; i >= 0; --i)
		unregister_hook(hook_funcs[i].type, hook_funcs[i].func);
}

void system_hooks_fini(void)
{
	uint i;

	for (i = 0; i < ARRAY_SIZE(hook_funcs); ++i)
		unregister_hook(hook_funcs[i].type, hook_funcs[i].func);
}