/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef _PMI_H
#define _PMI_H

#define NMI_NAME "RECODE_PMI"
#define PERF_COND_CHGD_IGNORE_MASK (BIT_ULL(63) - 1)

#define LVT_NMI		(0x4 << 8)
#define NMI_LINE	2

#define RECODE_PMI 239
#define PMI_DELAY 0x100

int pmi_setup(void);
void pmi_cleanup(void);

extern atomic_t active_pmis;

bool pmc_access_on_pmi_local(void);

void pmudrv_update_vector(int vector);

#endif /* _PMI_H */