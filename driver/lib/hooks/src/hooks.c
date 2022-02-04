// SPDX-License-Identifier: GPL-2.0

#include <linux/mm.h>
#include <linux/list.h>
#include <linux/tracepoint.h>

#include "hooks.h"

struct hook {
	char *name;
	void *func;
	enum hook_type type;
	struct tracepoint *tp;
	struct list_head list;
};

static LIST_HEAD(hooks_list);
static DEFINE_SPINLOCK(hooks_list_lock);

static char *get_tp_name(enum hook_type type)
{
	switch (type) {
	case SCHED_IN:
		return "sched_switch";
	case PROC_EXEC:
		return "sched_process_exec";
	case PROC_EXIT:
		return "sched_process_exit";
	case PROC_FORK:
		return "sched_process_fork";
	case PROC_FREE:
		return "sched_process_free";
	case SCHED_OUT: // Use ftrace
	default:
		return "";
	}
}

static void lookup_tracepoint(struct tracepoint *tp, void *hook_ptr)
{
	struct hook *hook = (struct hook *)hook_ptr;

	if (strcmp(hook->name, tp->name) == 0)
		hook->tp = tp;
}

int register_hook(enum hook_type type, void *func)
{
	struct hook *hook;

	if (!func)
		return -EINVAL;

	hook = kvmalloc(sizeof(struct hook), GFP_KERNEL);

	if (!hook)
		return -ENOMEM;

	hook->type = type;
	hook->func = func;
	hook->name = get_tp_name(hook->type);

	/* Fill the hook's tracepoint */
	for_each_kernel_tracepoint(lookup_tracepoint, hook);

	// TODO finish_task_switch -> ftrace

	if (!hook->tp) {
		pr_warn("Cannot find tracepoint for %s\n", hook->name);
		goto no_tp;
	}

	if (tracepoint_probe_register(hook->tp, hook->func, NULL)) {
		pr_warn("Error while registering thetracepoint %s\n",
			hook->name);
		goto trace_err;
	}

	spin_lock(&hooks_list_lock);
	list_add(&hook->list, &hooks_list);
	spin_unlock(&hooks_list_lock);

	pr_info("Register probe of type %s on tp %s\n", get_tp_name(type),
		hook->name);

	return 0;

trace_err:
no_tp:
	kvfree(hook);
	return -ENXIO;
}

void unregister_hook(enum hook_type type, void *func)
{
	struct hook *tmp = NULL;
	struct hook *pos = NULL;

	spin_lock(&hooks_list_lock);
	list_for_each_entry_safe(pos, tmp, &hooks_list, list) {
		if (pos->type == type && pos->func == func) {
			list_del(&pos->list);
			break;
		}
	}
	spin_unlock(&hooks_list_lock);

	tracepoint_probe_unregister(pos->tp, pos->func, NULL);
	tracepoint_synchronize_unregister();

	pr_info("Unregister probe of type %s\n", get_tp_name(type));

	kvfree(pos);
}
