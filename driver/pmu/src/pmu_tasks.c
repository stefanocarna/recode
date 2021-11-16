// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note

#include <linux/mm.h>
#include <linux/perf_event.h>

#include "pmu.h"
#include "pmu_abi.h"
#include "pmu_core.h"

#include "hooks.h"

struct pmu_tasks_data {
	bool profiled;
	bool removing;
};

static inline struct perf_event *__get_perf_event(struct task_struct *task)
{
	/* Should we use the mutex here? */
	return list_first_entry_or_null(&(task->perf_event_list),
					struct perf_event, owner_entry);
}

struct pmu_tasks_data *get_task_data(struct task_struct *task)
{
	struct pmu_tasks_data *pmu_tasks_data;
	struct perf_event *event = __get_perf_event(task);

	if (!event)
		return NULL;

	pmu_tasks_data = (struct pmu_tasks_data *)event->pmu_private;

	if (pmu_tasks_data->removing) {
		destroy_task_data(task);
		return NULL;
	}

	return pmu_tasks_data;
}

bool create_task_data(struct task_struct *task)
{
	struct perf_event *event;
	struct pmu_tasks_data *pmu_tasks_data;
	struct perf_event_attr attr = {
		.type = PERF_TYPE_SOFTWARE,
		.config = PERF_COUNT_SW_DUMMY,
		.size = sizeof(attr),
		.disabled = true,
	};

	if (!task)
		return false;

	event = perf_event_create_kernel_counter(&attr, -1, task, NULL, NULL);

	if (IS_ERR(event)) {
		pr_info("CANNOT MOCKUP thread data. perf_event_create failed: %ld\n",
			PTR_ERR(event));
		return false;
	}

	pmu_tasks_data = kvzalloc(sizeof(*pmu_tasks_data), GFP_KERNEL);

	if (!pmu_tasks_data)
		return false;

	event->pmu_private = pmu_tasks_data;

	mutex_lock(&task->perf_event_mutex);
	list_add_tail(&event->owner_entry, &task->perf_event_list);
	mutex_unlock(&task->perf_event_mutex);

	return true;
}

bool exist_or_create_task_data(struct task_struct *task)
{
	return !!__get_perf_event(task) || create_task_data(task);
}

void destroy_task_data(struct task_struct *task)
{
	struct perf_event *event = __get_perf_event(task);

	if (!event)
		return;

	if (event->pmu_private)
		kvfree(event->pmu_private);

	perf_event_release_kernel(event);
}

int track_thread(struct task_struct *task)
{
	struct pmu_tasks_data *pmu_tasks_data;

	if (!task)
		return -EINVAL;

	get_task_struct(task);

	pmu_tasks_data = get_task_data(task);

	/* Already tracked */
	if (pmu_tasks_data)
		return -EPERM;

	create_task_data(task);

	// pmu_tasks_data->profiled = true;

	put_task_struct(task);

	return 0;
}
EXPORT_SYMBOL(track_thread);

bool query_tracked(struct task_struct *task)
{
	struct pmu_tasks_data *pmu_tasks_data;

	if (!task)
		return false;

	get_task_struct(task);

	pmu_tasks_data = get_task_data(task);

	if (!pmu_tasks_data)
		return false;

	put_task_struct(task);

	return true;
}
EXPORT_SYMBOL(query_tracked);

void untrack_thread(struct task_struct *task)
{
	struct pmu_tasks_data *pmu_tasks_data;

	if (!task)
		return;

	get_task_struct(task);

	pmu_tasks_data = get_task_data(task);

	if (!pmu_tasks_data)
		return;

	/* Remove inplace or delay if it is another thread */
	if (current->pid == task->pid)
		destroy_task_data(task);
	else
		pmu_tasks_data->removing = true;

	put_task_struct(task);
}
EXPORT_SYMBOL(untrack_thread);
