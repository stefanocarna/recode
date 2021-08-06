#include "multiplexer.h"

void pmc_collect_partial_values(unsigned cpu, unsigned cnt, unsigned index)
{
	unsigned pmc;
	u64 ctrl = per_cpu(pcpu_pmus_metadata.perf_global_ctrl, cpu);
	struct pmcs_collection *pmcs_collection =
		per_cpu(pcpu_pmus_metadata.pmcs_collection, cpu);

	if (unlikely(!pmcs_collection)) {
		pr_debug("pmcs_collection is null\n");
		return;
	}

	if (index == 0)
		pmcs_collection->complete = false;

	/* Collect fixed values only at the end of the multiplexing period */
	// TODO - Fixed required adjustment (OLD = NEW - OLD)
	if ((cnt - index) <= gbl_nr_pmc_general) {
		for_each_active_fixed_pmc (ctrl, pmc) {
			pmcs_fixed(pmcs_collection->pmcs)[pmc] =
				READ_FIXED_PMC(pmc);
		}
		pmcs_collection->cnt = gbl_nr_pmc_fixed + cnt;
		pmcs_collection->complete = true;
	}

	for_each_active_general_pmc (ctrl, pmc) {
		pmcs_general(pmcs_collection->pmcs)[index++] =
			READ_GENERAL_PMC(pmc);
	}
}

/* Called with preemption off */
bool pmc_multiplexing_on_pmi(unsigned cpu)
{
	unsigned index = per_cpu(pcpu_pmus_metadata.hw_events_index, cpu);

	struct hw_events *hw_events =
		per_cpu(pcpu_pmus_metadata.hw_events, cpu);

	if (!hw_events) {
		pr_warn("Processing pmi on cpu %u without hw_events\n", cpu);
		return false;
	}

	unsigned req_hw_events = (hw_events->cnt - index);

	/* TODO - Introduce TSC check to see if the event can be precise */

	pr_debug("Multiplexing on %u events on cpu %u - GPCs %u\n",
		 req_hw_events, cpu, gbl_nr_pmc_general);

	pmc_collect_partial_values(cpu, hw_events->cnt, index);

	/* enough pmcs for events */
	if (gbl_nr_pmc_general >= req_hw_events) {
		fast_setup_general_pmc_on_cpu(cpu, hw_events->cfgs, index,
					      req_hw_events);
		per_cpu(pcpu_pmus_metadata.hw_events_index, cpu) = 0;

		return true;
	} else {
		fast_setup_general_pmc_on_cpu(cpu, hw_events->cfgs, index,
					      index + gbl_nr_pmc_general);
		/* NOTE - This value should always be smaller than "cnt" */
		per_cpu(pcpu_pmus_metadata.hw_events_index, cpu) =
			index + gbl_nr_pmc_general;

		return false;
	}
}
