
#ifndef _RECODE_PMI_H
#define _RECODE_PMI_H

#define NMI_NAME "RECODE_PMI"
#define PERF_COND_CHGD_IGNORE_MASK (BIT_ULL(63) - 1)

#define LVT_NMI		(0x4 << 8) 
#define NMI_LINE	2

#define RECODE_PMI 239

// extern atomic_t active_pmis;

int pmi_nmi_setup(void);
void pmi_nmi_cleanup(void);

int pmi_irq_setup(void);
void pmi_irq_cleanup(void);

#endif /* _RECODE_PMI_H */