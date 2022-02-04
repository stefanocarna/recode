/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#include "pmu.h"
#include "hw_events.h"
#include "pmu_structs.h"
#include "pmu_low.h"
#include "logic/tma.h"

int register_on_pmi_callback(pmi_callback * callback);