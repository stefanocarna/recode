#ifndef _RECODE_TMA_H
#define _RECODE_TMA_H

#include "recode_collector.h"
#include "pmu/pmu.h"

/** 
 * __COUNTER__ is a GCC preprocessor macro that is accept by clang.
 * We use this macro to initizialize and keep sorted the array of events
 * and to allow an easy integration of further elements.
*/
enum { __X_COUNTER__ = __COUNTER__ };
#define __CTR__ (__COUNTER__ - __X_COUNTER__ - 1)

struct tma_event {
	unsigned idx;
	pmc_evt_code evt;
};

#define COMPOSE_TMA_EVT(name)                                                  \
	const struct tma_event tma_evt_##name = { .idx = __CTR__,                    \
					    .evt = { evt_##name } }


int recode_tma_init(void);

void recode_tma_fini(void);

void update_events_index_on_this_cpu(struct hw_events *events);

void compute_tma(struct pmcs_collection *collection, u64 mask, u8 cpu);

#endif /* _RECODE_TMA_H */
