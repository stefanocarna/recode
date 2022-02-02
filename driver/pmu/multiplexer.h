#ifndef _MULTIPLEXER_H
#define _MULTIPLEXER_H

#include "pmu.h"

bool pmc_multiplexing_on_pmi(unsigned cpu);
bool pmc_access_on_pmi_local(unsigned cpu);

#endif /* _MULTIPLEXER_H */
