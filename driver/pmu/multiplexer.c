// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note

#include <linux/sched.h>

#include "pmu_structs.h"
#include "pmu.h"
//#include "pmu_low.h"
#include "pmi.h"

uint pmc_collect_partial_values(uint gp_cnt, uint index)
{
	uint pmc, hw_cnt;
	pmc_ctr value;
	pmc_ctr *hw_pmcs = this_cpu_read(pcpu_pmus_metadata.hw_pmcs);
	u64 ctrl = this_cpu_read(pcpu_pmus_metadata.perf_global_ctrl);
	struct pmcs_collection *pmcs_collection =
		this_cpu_read(pcpu_pmus_metadata.pmcs_collection);

	if (unlikely(!pmcs_collection)) {
		pr_debug("pmcs_collection is null\n");
		return 0;
	}

	hw_cnt = gp_cnt - index;

	// pr_info("hw_cnt: %u", hw_cnt);

	/* Collect fixed values only at the end of the multiplexing period */
	if (hw_cnt <= gbl_nr_pmc_general) {
		for_each_active_fixed_pmc (ctrl, pmc) {
			/* NEW value */
			value = READ_FIXED_PMC(pmc);
			/* Compute current sample value */

			/* Compute the increment and update the old value */
			pmcs_fixed(pmcs_collection->pmcs)[pmc] =
				value - pmcs_fixed(hw_pmcs)[pmc];

			pr_debug(
				"** [PMC-SLOW] FIXED %u [%llx -> %llx = %llx]\n",
				pmc, pmcs_fixed(hw_pmcs)[pmc], value,
				pmcs_fixed(pmcs_collection->pmcs)[pmc]);

			pmcs_fixed(hw_pmcs)[pmc] = value;
		}

		pmcs_collection->cnt = gbl_nr_pmc_fixed + gp_cnt;

		/* TODO This may be local in the future */
		pmcs_fixed(pmcs_collection->pmcs)[gbl_fixed_pmc_pmi] =
			gbl_reset_period;

	} else {
		hw_cnt = gbl_nr_pmc_general;
	}

	/* Compute only required pmcs */
	for_each_active_pmc(ctrl, pmc, 0, hw_cnt) {
		pmcs_general(pmcs_collection->pmcs)[index++] =
			READ_GENERAL_PMC(pmc);

		// TODO Remove
		// if (hw_cnt <= gbl_nr_pmc_general)
		// 	pr_info("READ PMC %u @ index %u\n", pmc, index);
	}

	this_cpu_inc(pcpu_pmus_metadata.pmi_partial_cnt);

	return index;
}

static void finalize_multiplexing(u64 mask)
{
	pmc_ctr tsc;
	uint pmc;
	uint scale = this_cpu_read(pcpu_pmus_metadata.pmi_partial_cnt);
	struct pmcs_collection *pmcs_collection =
		this_cpu_read(pcpu_pmus_metadata.pmcs_collection);

	/* Collection */
	for_each_pmc(pmc, nr_pmcs_general(pmcs_collection->cnt))
		pmcs_general(pmcs_collection->pmcs)[pmc] *= scale;

	pmcs_collection->mask = mask;

	/* TSC */
	tsc = rdtsc_ordered();
	this_cpu_write(pcpu_pmus_metadata.sample_tsc, tsc - this_cpu_read(pcpu_pmus_metadata.last_tsc));
	this_cpu_write(pcpu_pmus_metadata.last_tsc, tsc);

	/* CPU reset */
	this_cpu_write(pcpu_pmus_metadata.pmi_partial_cnt, 0);
	this_cpu_write(pcpu_pmus_metadata.hw_events_index, 0);
}

static bool pmc_multiplexing_access(void)
{
	uint new_idx;
	uint nr_hw_events;
	uint residual_hw_events;
	uint old_idx = this_cpu_read(pcpu_pmus_metadata.hw_events_index);
	bool completed = false;

	struct hw_events *hw_events =
		this_cpu_read(pcpu_pmus_metadata.hw_events);

	// pr_info("%s on CPU %u - pid %u\n", __func__, cpu, current->pid);

	if (!hw_events) {
		pr_warn("Processing pmi on cpu %u without hw_events\n", smp_processor_id());
		return false;
	}

	nr_hw_events = hw_events->cnt;

	/* Initial case */
	if (old_idx == 0) {
		new_idx =
			pmc_collect_partial_values(nr_hw_events, old_idx);
		residual_hw_events = nr_hw_events - new_idx;
	} else {
		/* Middle case */
		new_idx =
			pmc_collect_partial_values(nr_hw_events, old_idx);
		residual_hw_events = nr_hw_events - new_idx;
	}

	/* Final case */
	if (residual_hw_events == 0) {
		/* Multiplexing ends here */
		finalize_multiplexing(hw_events->mask);
		completed = true;
		/* Set starting values */
		new_idx = 0;
		residual_hw_events = nr_hw_events;
	}

	/* NOTE this crop is already done inside the setup routine */
	if (residual_hw_events > gbl_nr_pmc_general)
		residual_hw_events = gbl_nr_pmc_general;

	/* Set next multiplexing session */
	//fast_setup_general_pmc_local(hw_events->cfgs, new_idx, residual_hw_events);
	fast_setup_general_pmc_on_cpu(smp_processor_id(), hw_events->cfgs, new_idx, residual_hw_events);

	this_cpu_write(pcpu_pmus_metadata.hw_events_index, new_idx);

	return completed;
	/* TODO - Introduce TSC check to see if the event can be precise */

	/* Read from index to cnt */

	/* Check hw_events to be set up */

	// pr_info("[%u] REQ %u) Multiplexed %u/%u events GPCs %u - index %u\n",
	// 	 cpu, req_hw_events, index, hw_events->cnt, gbl_nr_pmc_general,
	// 	 index);

	// if (this_cpu_read(pcpu_pmus_metadata.ctx_cnt)) {
	// 	pr_info("@%u reset_times: %llu\n", smp_processor_id(),
	// 		this_cpu_read(pcpu_pmus_metadata.ctx_cnt));
	// 	this_cpu_write(pcpu_pmus_metadata.ctx_cnt, 0);
	// }

	/* Required preemption off */
	// pr_warn("%u] SETUP index %u - req %u\n", cpu, index, req_hw_events);
}
/*
static bool pmc_fast_access(void)
{
	uint pmc;
	pmc_ctr tsc;
	pmc_ctr value;
	pmc_ctr *hw_pmcs = this_cpu_read(pcpu_pmus_metadata.hw_pmcs);
	u64 ctrl = this_cpu_read(pcpu_pmus_metadata.perf_global_ctrl);
	struct hw_events *hw_events =
		this_cpu_read(pcpu_pmus_metadata.hw_events);
	struct pmcs_collection *pmcs_collection =
		this_cpu_read(pcpu_pmus_metadata.pmcs_collection);

	// pr_info("%s on CPU %u - pid %u\n", __func__, cpu, current->pid);

	if (unlikely(!hw_events || !pmcs_collection)) {
		pr_warn("Processing pmi on cpu %u without data structs\n",
			smp_processor_id());
		return false;
	}

	for_each_active_fixed_pmc(ctrl, pmc) {
		// NEW value
		value = READ_FIXED_PMC(pmc);

		// Compute the increment and update the old value
		pmcs_fixed(pmcs_collection->pmcs)[pmc] =
			value - pmcs_fixed(hw_pmcs)[pmc];

		pr_debug("** [PMC-FAST] FIXED %u [%llx -> %llx = %llx]\n", pmc,
			 pmcs_fixed(hw_pmcs)[pmc], value,
			 pmcs_fixed(pmcs_collection->pmcs)[pmc]);

		pmcs_fixed(hw_pmcs)[pmc] = value;
	}

	// Adjust the PMI pmc value
	pmcs_fixed(pmcs_collection->pmcs)[gbl_fixed_pmc_pmi] = gbl_reset_period;

	// Compute only required pmcs
	for_each_active_pmc(ctrl, pmc, 0, gbl_nr_pmc_general) {
		// NEW value
		value = READ_GENERAL_PMC(pmc);

		// Compute the increment and update the old value
		pmcs_general(pmcs_collection->pmcs)[pmc] =
			value - pmcs_general(hw_pmcs)[pmc];

		pr_debug("** [PMC-FAST] GENERAL %u [%llx -> %llx : %llx]\n",
			 pmc, pmcs_general(hw_pmcs)[pmc], value,
			 pmcs_general(pmcs_collection->pmcs)[pmc]);

		pmcs_general(hw_pmcs)[pmc] = value;
	}

	tsc = rdtsc_ordered();
	this_cpu_write(pcpu_pmus_metadata.sample_tsc,
		       tsc - this_cpu_read(pcpu_pmus_metadata.last_tsc));
	this_cpu_write(pcpu_pmus_metadata.last_tsc, tsc);

	pmcs_collection->cnt = gbl_nr_pmc_fixed + hw_events->cnt;

	return true;
}
*/

/* Must be called with preemption off */
bool pmc_access_on_pmi_local(void)
{
	return pmc_multiplexing_access();
}
