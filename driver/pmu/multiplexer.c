#include "multiplexer.h"

unsigned pmc_collect_partial_values(unsigned cpu, unsigned gp_cnt,
				    unsigned index)
{
	unsigned pmc, hw_cnt;
	pmc_ctr value;
	u64 ctrl = per_cpu(pcpu_pmus_metadata.perf_global_ctrl, cpu);
	struct pmcs_collection *pmcs_collection =
		per_cpu(pcpu_pmus_metadata.pmcs_collection, cpu);

	if (unlikely(!pmcs_collection)) {
		pr_debug("pmcs_collection is null\n");
		return 0;
	}

	if (index == 0)
		pmcs_collection->complete = false;

	hw_cnt = gp_cnt - index;

	/* Collect fixed values only at the end of the multiplexing period */
	if (hw_cnt <= gbl_nr_pmc_general) {
		for_each_active_fixed_pmc (ctrl, pmc) {
			/* NEW value */
			value = READ_FIXED_PMC(pmc);
			/* Compute current sample value */

			pmcs_fixed(pmcs_collection->pmcs)[pmc] = value;

			WRITE_FIXED_PMC(pmc, 0ULL);
			
			// TODO - FIX & RESTORE!
			// pmcs_fixed(pmcs_collection->pmcs)[pmc] =
			// 	value -
			// 	this_cpu_read(
			// 		pcpu_pmus_metadata.pmcs_fixed)[pmc];
			
			// pr_debug("** $$ FIXED - old %llx - new %llx\n", this_cpu_read(
			// 		pcpu_pmus_metadata.pmcs_fixed)[pmc], value);
			
			// /* Update OLD Value */
			// this_cpu_read(pcpu_pmus_metadata.pmcs_fixed)[pmc] =
			// 	value;

		}

		pmcs_collection->cnt = gbl_nr_pmc_fixed + gp_cnt;
		pmcs_collection->complete = true;

		pmcs_fixed(pmcs_collection->pmcs)[gbl_fixed_pmc_pmi] =
			gbl_reset_period;
	} else {
		hw_cnt = gbl_nr_pmc_general;
	}

	/* Compute only required pmcs */
	for_each_active_pmc (ctrl, pmc, 0, hw_cnt) {
		pmcs_general(pmcs_collection->pmcs)[index++] =
			READ_GENERAL_PMC(pmc);
	}

	per_cpu(pcpu_pmus_metadata.pmi_partial_cnt, cpu)++;
	/* Update value of index */
	return index;
}

/* Called with preemption off */
bool pmc_multiplexing_on_pmi(unsigned cpu)
{
	unsigned req_hw_events;
	unsigned index = per_cpu(pcpu_pmus_metadata.hw_events_index, cpu);

	struct hw_events *hw_events =
		per_cpu(pcpu_pmus_metadata.hw_events, cpu);

	if (!hw_events) {
		pr_warn("Processing pmi on cpu %u without hw_events\n", cpu);
		return false;
	}

	/* TODO - Introduce TSC check to see if the event can be precise */

	/* Read from index to cnt */
	index = pmc_collect_partial_values(cpu, hw_events->cnt, index);

	/* Check hw_events to be set up */
	req_hw_events = (hw_events->cnt - index);

	pr_debug("[%u] REQ %u) Multiplexed %u/%u events GPCs %u - index %u\n",
		 cpu, req_hw_events, index, hw_events->cnt, gbl_nr_pmc_general,
		 index);

	/* enough pmcs for events or completed multiplexing */
	if (req_hw_events == 0) {
		unsigned k, pmc;
		unsigned scale =
			per_cpu(pcpu_pmus_metadata.pmi_partial_cnt, cpu);
		struct pmcs_collection *pmcs_collection =
			per_cpu(pcpu_pmus_metadata.pmcs_collection, cpu);

		pr_debug("BEFORE SCALING SAMPLE: ");
		for (k = 0; k < gbl_nr_pmc_fixed; ++k) {
			pr_cont("%llu ", pmcs_collection->pmcs[k]);
		}
		pr_cont(" - ");
		for (k = gbl_nr_pmc_fixed; k < pmcs_collection->cnt; ++k) {
			pr_cont("%llu ", pmcs_collection->pmcs[k]);
		}
		pr_cont("\n");

		for_each_pmc (pmc, nr_pmcs_general(pmcs_collection->cnt)) {
			pmcs_general(pmcs_collection->pmcs)[pmc] *= scale;
		}

		// for_each_pmc (pmc, pmcs_collection->cnt) {
		// 	pr_debug("pmc %u: %llx\n", pmc,
		// 		 pmcs_collection->pmcs[pmc]);
		// }

		if (hw_events->cnt > gbl_nr_pmc_general)
			req_hw_events = gbl_nr_pmc_general;
		else
			req_hw_events = hw_events->cnt;

		/* Setup from  */
		fast_setup_general_pmc_on_cpu(cpu, hw_events->cfgs, 0,
					      req_hw_events);

		per_cpu(pcpu_pmus_metadata.hw_events_index, cpu) = 0;
		per_cpu(pcpu_pmus_metadata.pmi_partial_cnt, cpu) = 0;

		return true;
	} else {
		if (req_hw_events > gbl_nr_pmc_general)
			req_hw_events = gbl_nr_pmc_general;

		fast_setup_general_pmc_on_cpu(cpu, hw_events->cfgs, index,
					      req_hw_events);
		/* NOTE - This value should always be smaller than "cnt" */
		per_cpu(pcpu_pmus_metadata.hw_events_index, cpu) =
			index;

		return false;
	}
}
