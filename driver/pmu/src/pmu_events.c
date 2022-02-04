#include <linux/mm.h>

#include "pmu_config.h"
#include "pmu_low.h"
#include "pmu.h"
#include "pmi.h"
#include "hw_events.h"
#include "logic/tma.h"

static void dummy_hw_events_change_callback(struct hw_events *events)
{
	/* Empty */
}

void (*on_hw_events_setup_callback)(struct hw_events *events) =
	dummy_hw_events_change_callback;

// void (*on_hw_events_setup_callback)(struct hw_events *events);

/**
 * This macro creates an instance of struct hw_event for each hw_event present
 * inside HW_EVENTS. That struct is used to dynamically compute the mask of the
 * hw_events_set and other utility functions.
 */
#define X_HW_EVENTS(i, name, code)                                             \
	const struct hw_event hw_evt_##name = { .idx = i, .evt = { code } };   \
	EXPORT_SYMBOL(hw_evt_##name);
HW_EVENTS
#undef X_HW_EVENTS

/* Define an array containing all the hw_events present inside HW_EVENTS. */
#define X_HW_EVENTS(i, name, code) { HW_EVT_COD(name) },
const pmc_evt_code gbl_raw_events[NR_HW_EVENTS] = { HW_EVENTS };
#undef X_HW_EVENTS

/* Preemption must be disabled */
void fast_setup_general_pmc_local(struct pmc_evt_sel *pmc_cfgs, uint off,
				  uint cnt)
{
	u64 ctrl;
	uint pmc;

	/* Avoid to write wrong MSRs */
	if (cnt > gbl_nr_pmc_general)
		cnt = gbl_nr_pmc_general;

	ctrl = FIXED_PMCS_TO_BITS_MASK | GENERAL_PMCS_TO_BITS_MASK(cnt);

	/* Unneeded PMCs are disabled in ctrl */
	for_each_active_general_pmc(ctrl, pmc)
	{
		// TODO Check this write and pmc_cfgs allocation
		SETUP_GENERAL_PMC(pmc, pmc_cfgs[pmc + off].perf_evt_sel);
		WRITE_GENERAL_PMC(pmc, 0ULL);
	}

	this_cpu_write(pcpu_pmus_metadata.perf_global_ctrl, ctrl);
}

static void __update_reset_period_local(void)
{
	pmc_ctr reset_period;
	uint multiplexing = this_cpu_read(pcpu_pmus_metadata.multiplexing);


	if (multiplexing)
		reset_period = gbl_reset_period / multiplexing;
	else
		reset_period = gbl_reset_period;

	reset_period = PMC_TRIM(~reset_period);

	this_cpu_write(pcpu_pmus_metadata.pmi_reset_value, reset_period);

	pr_debug("[%u] (GBL :%llx) Reset period set: %llx - Multiplexing times: %u\n",
		smp_processor_id(), gbl_reset_period, reset_period, multiplexing);
}

static void __update_reset_period_local_smp(void *unused)
{
	__update_reset_period_local();
}

void update_reset_period_global(pmc_ctr reset_period)
{
	disable_pmcs_global();

	gbl_reset_period = reset_period;

	on_each_cpu(__update_reset_period_local_smp, NULL, 1);

	pr_info("Updated gbl_reset_period to %llx\n", reset_period);

	// TODO create a global state
	if (pmu_enabled)
		enable_pmcs_global();
}

/* Require PMCs and PREEMPTION off */
static void __reset_hw_events_local(void)
{
	uint pmc;
	struct hw_events *hw_events =
		this_cpu_read(pcpu_pmus_metadata.hw_events);

	/* TODO Check if it requires synchronization (RCU?) */
	if (!hw_events)
		return;

	/* Reset Temporary PMC Container */
	memset(this_cpu_read(pcpu_pmus_metadata.hw_pmcs), 0,
	       array_size(sizeof(pmc_ctr), NR_SYSTEM_PMCS));

	this_cpu_write(pcpu_pmus_metadata.pmi_partial_cnt, 0);
	this_cpu_write(pcpu_pmus_metadata.hw_events_index, 0);

	/* Reset GENERAL pmcs */
	fast_setup_general_pmc_local(hw_events->cfgs, 0, hw_events->cnt);

	/* Reset FIXED pmcs */
	for_each_fixed_pmc(pmc) WRITE_FIXED_PMC(pmc, 0);

	WRITE_FIXED_PMC(gbl_fixed_pmc_pmi,
			this_cpu_read(pcpu_pmus_metadata.pmi_reset_value));

	this_cpu_inc(pcpu_pmus_metadata.ctx_cnt);
}

void reset_hw_events_local(void)
{
	bool state;

	save_and_disable_pmcs_local(&state);
	__reset_hw_events_local();
	restore_and_enable_pmcs_local(&state);
}

/* Required preemption disabled */
void setup_hw_events_local(struct hw_events *hw_events)
{
	/* TODO improve avoid processing already scheduled hw_events */
	uint tmp;
	uint hw_cnt;
	bool state;
	bool mpx_round;
	struct pmcs_collection *pmcs_collection;

	if (!hw_events || !hw_events->cnt) {
		pr_warn("Cannot setup hw_events on cpu %u: NULL or ZERO evts\n",
			smp_processor_id());
		return;
	}

	save_and_disable_pmcs_local(&state);

	pr_info("** %s @ %u\n", __func__, smp_processor_id());

	hw_cnt = hw_events->cnt;

	pmcs_collection = this_cpu_read(pcpu_pmus_metadata.pmcs_collection);

	pmcs_collection->cnt = hw_cnt + gbl_nr_pmc_fixed;
	pmcs_collection->mask = ((struct hw_events *)hw_events)->mask;

	/* Update hw_events */
	this_cpu_write(pcpu_pmus_metadata.hw_events, hw_events);
	/* Overcoming FPU "lack" - hw_cnt cannot be zero */
	tmp = hw_cnt / gbl_nr_pmc_general;
	mpx_round = (tmp * gbl_nr_pmc_general) != hw_cnt;
	this_cpu_write(pcpu_pmus_metadata.multiplexing,
		       mpx_round ? tmp + 1 : tmp);

	/* TMA */
	/* Update pmc index array */
	update_events_index_local(hw_events);

	// on_hw_events_setup_callback(hw_events);

	__update_reset_period_local();

	__reset_hw_events_local();

	restore_and_enable_pmcs_local(&state);
}
EXPORT_SYMBOL(setup_hw_events_local);

static void __setup_hw_events_local_smp(void *hw_events)
{
	preempt_disable();
	setup_hw_events_local((struct hw_events *)hw_events);
	preempt_enable();
}

struct hw_events *create_hw_events(pmc_evt_code *codes, uint cnt)
{
	u64 mask;
	uint i, b;
	struct hw_events *hw_events;

	if (!codes || !cnt) {
		pr_warn("Invalid codes. Cannot create hw_events\n");
		return NULL;
	}

	mask = compute_hw_events_mask(codes, cnt);

	hw_events = kvzalloc(sizeof(struct hw_events) +
				     (sizeof(struct pmc_evt_sel) * cnt),
			     GFP_KERNEL);

	if (!hw_events) {
		pr_warn("Cannot allocate memory for hw_events\n");
		return NULL;
	}

	/* Remove duplicates */
	for (i = 0, b = 0; b < 64; ++b) {
		if (!(mask & BIT(b)))
			continue;

		hw_events->cfgs[i].perf_evt_sel = gbl_raw_events[b].raw;
		/* PMC setup */
		hw_events->cfgs[i].usr = !!(params_cpl_usr);
		hw_events->cfgs[i].os = !!(params_cpl_os);
		hw_events->cfgs[i].pmi = 0;
		hw_events->cfgs[i].en = 1;
		pr_info("Configure HW_EVENT %llx\n",
			hw_events->cfgs[i].perf_evt_sel);

		++i;
	}

	hw_events->cnt = i;
	hw_events->mask = mask;

	pr_info("Created hw_events MASK: %llx (cnt %u)\n", hw_events->mask,
		hw_events->cnt);

	return hw_events;
}
EXPORT_SYMBOL(create_hw_events);

void destroy_hw_events(struct hw_events *hw_events)
{
	if (!hw_events)
		return;

	kvfree(hw_events);
}
EXPORT_SYMBOL(destroy_hw_events);

int setup_hw_events_global(struct hw_events *hw_events)
{
	if (!hw_events) {
		pr_warn("Invalid hw_events. Cannot proceed with setup\n");
		return -EINVAL;
	}

	on_each_cpu(__setup_hw_events_local_smp, hw_events, 1);

	return 0;
}
EXPORT_SYMBOL(setup_hw_events_global);

u64 compute_hw_events_mask(pmc_evt_code *hw_events_codes, uint cnt)
{
	u64 mask = 0;
	uint e, k;

	for (e = 0; e < cnt; ++e) {
		for (k = 0; k < NR_HW_EVENTS; ++k) {
			if (hw_events_codes[e].raw == gbl_raw_events[k].raw) {
				mask |= BIT(k);
				break;
			}
		}
	}

	return mask;
}
