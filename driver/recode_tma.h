#ifndef _RECODE_TMA_H
#define _RECODE_TMA_H

#include "recode_collector.h"
#include "pmu/pmu.h"

#define TMA_L0_THRESHOLD  1
#define TMA_L1_THRESHOLD  2

void update_index_array(struct hw_events *events);

void compute_tma(struct pmcs_collection *collection, u64 mask, u8 cpu);

#endif /* _RECODE_TMA_H */
