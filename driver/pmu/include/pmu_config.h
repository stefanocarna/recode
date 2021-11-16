/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef _PMU_CONFIG_H
#define _PMU_CONFIG_H

#include <asm/cache.h>
#include <linux/types.h>

#define MODNAME	"PMULight"

#undef pr_fmt
#define pr_fmt(fmt) MODNAME ": " fmt

enum pmi_vector {
	NMI,
#ifdef FAST_IRQ_ENABLED
	IRQ
#endif
};

extern enum pmi_vector pmi_vector;

extern u64 __read_mostly gbl_reset_period;

/* TODO Refactor */
extern bool params_cpl_os;
extern bool params_cpl_usr;

#endif /* _PMU_CONFIG_H */
