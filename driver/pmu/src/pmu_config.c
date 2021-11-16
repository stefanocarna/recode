// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note

#include "pmu_config.h"

#include <linux/bits.h>

#define SAMPLING_PERIOD (BIT_ULL(25) - 1)
enum pmi_vector pmi_vector = NMI;

uint __read_mostly gbl_fixed_pmc_pmi = 2; // PMC with PMI active

u64 __read_mostly gbl_reset_period = SAMPLING_PERIOD;

bool params_cpl_os = 1;
bool params_cpl_usr = 1;
