#ifndef _RECODE_CONFIG_H
#define _RECODE_CONFIG_H

#include "pmu/pmu.h"

extern u64 __read_mostly reset_period;
extern unsigned __read_mostly fixed_pmc_pmi;

/* TODO Refactor */
extern unsigned params_cpl_os;
extern unsigned params_cpl_usr;

#define RING_BUFF_LENGTH 12
#define RING_SIZE (sizeof(struct data_logger_ring))

#define RING_COUNT 128
#define BUFF_MEMORY RING_COUNT * RING_SIZE

enum recode_pmi_vector {
	NMI,
#ifdef FAST_IRQ_ENABLED
	IRQ
#endif
};

extern enum recode_pmi_vector recode_pmi_vector;

#define DECLARE_PMC_EVT_CODES(n, s) extern pmc_evt_code n[s]

DECLARE_PMC_EVT_CODES(pmc_events_management, 8);

DECLARE_PMC_EVT_CODES(pmc_events_tma_l0, 8);
DECLARE_PMC_EVT_CODES(pmc_events_tma_l1, 8);
DECLARE_PMC_EVT_CODES(pmc_events_tma_l2, 8);

#endif /* _RECODE_CONFIG_H */