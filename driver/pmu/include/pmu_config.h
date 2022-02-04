/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef _PMU_CONFIG_H
#define _PMU_CONFIG_H

#include <asm/cache.h>
#include <linux/types.h>

#define MODNAME	"PMULight"

#undef pr_fmt
#define pr_fmt(fmt) MODNAME ": " fmt

#if __has_include(<asm/fast_irq.h>)
#include <asm/fast_irq.h>
#define FAST_IRQ_ENABLED 1
#endif

enum pmi_vector {
	NMI = 0,
#ifdef FAST_IRQ_ENABLED
	IRQ = 1,
	MAX_VECTOR = 2
#else
	MAX_VECTOR = 1
#endif
};

extern enum pmi_vector pmi_vector;

extern u64 __read_mostly gbl_reset_period;

/* TODO Refactor */
extern bool params_cpl_os;
extern bool params_cpl_usr;

#endif /* _PMU_CONFIG_H */
