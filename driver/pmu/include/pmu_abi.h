/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#include "pmu.h"
#include "hw_events.h"
#include "pmu_structs.h"
#include "pmu_low.h"

int register_on_pmi_callback(pmi_callback * callback);

int register_on_hw_events_setup_callback(hw_events_change_callback *callback);

int track_thread(struct task_struct *task);

bool query_tracked(struct task_struct *task);

void untrack_thread(struct task_struct *task);
