
#ifdef FAST_IRQ_ENABLED
#include <asm/fast_irq.h>
#endif
#include <asm/msr.h>

#include <linux/percpu-defs.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/stringhash.h>

#include <asm/tsc.h>

#include "dependencies.h"
#include "device/proc.h"

#include "recode.h"
#include "pmu/pmi.h"
#include "recode_config.h"
#include "recode_collector.h"
#include "recode_tma.h"

DEFINE_PER_CPU(struct pmcs_snapshot, pcpu_pmcs_snapshot) = { 0 };
DEFINE_PER_CPU(bool, pcpu_last_ctx_snapshot) = false;

enum recode_state __read_mostly recode_state = OFF;

DEFINE_PER_CPU(struct pmcs_snapshot, pcpu_pmc_snapshot_ctx);
DEFINE_PER_CPU(struct data_logger *, pcpu_data_logger);

void pmc_generate_snapshot(struct pmcs_snapshot *old_pmcs, bool pmc_off);

int recode_data_init(void)
{
	unsigned cpu;

	for_each_online_cpu (cpu) {
		per_cpu(pcpu_data_collector, cpu) = init_collector(cpu);
		if (!per_cpu(pcpu_data_collector, cpu))
			goto mem_err;
	}

	return 0;
mem_err:
	pr_info("failed to allocate percpu pcpu_pmc_buffer\n");

	while (--cpu)
		fini_collector(cpu);

	return -1;
}

void recode_data_fini(void)
{
	unsigned cpu;

	for_each_online_cpu (cpu) {
		fini_collector(cpu);
	}
}

int recode_pmc_init(void)
{
	int err = 0;
	/* Setup fast IRQ */

	if (recode_pmi_vector == NMI) {
		err = pmi_nmi_setup();
	} else {
		err = pmi_irq_setup();
	}

	if (err) {
		pr_err("Cannot initialize PMI vector\n");
		goto err;
	}

	/* READ MACHINE CONFIGURATION */
	get_machine_configuration();

	// if (setup_pmc_on_system(pmc_events_management))
	if (init_pmu_on_system())
		goto no_cfgs;

	disable_pmc_on_system();

	recode_tma_init();

	on_each_cpu(setup_hw_events_on_cpu, gbl_hw_evts_groups[0], 1);

	return err;
no_cfgs:
	if (recode_pmi_vector == NMI) {
		pmi_nmi_cleanup();
	} else {
		pmi_irq_cleanup();
	}
err:
	return -1;
}

void recode_pmc_fini(void)
{
	/* Disable Recode */
	recode_state = OFF;

	disable_pmc_on_system();

	/* Wait for all PMIs to be completed */
	while (atomic_read(&active_pmis))
		;

	cleanup_pmc_on_system();

	if (recode_pmi_vector == NMI) {
		pmi_nmi_cleanup();
	} else {
		pmi_irq_cleanup();
	}

	pr_info("PMI uninstalled\n");
}

static void recode_reset_data(void)
{
	unsigned cpu;
	// unsigned pmc;

	for_each_online_cpu (cpu) {
		// reset_logger(per_cpu(pcpu_data_logger, cpu));

		/* TODO - Check when need to reset pcpu_pmcs_snapshot */

		// for (pmc = 0; pmc <= gbl_nr_pmc_fixed; ++pmc)
		// 	per_cpu(pcpu_pmcs_snapshot.fixed[pmc], cpu) = 0;

		// if (gbl_fixed_pmc_pmi <= gbl_nr_pmc_fixed)
		// 	per_cpu(pcpu_pmcs_snapshot.fixed[gbl_fixed_pmc_pmi], cpu) =
		// 		PMC_TRIM(reset_period);

		// for (pmc = 0; pmc < gbl_nr_pmc_general; ++pmc)
		// 	per_cpu(pcpu_pmcs_snapshot.general[pmc], cpu) = 0;
	}
}

static void on_context_switch_callback(struct task_struct *prev, bool prev_on,
				       bool curr_on)
{
	unsigned cpu = get_cpu();
	// struct data_logger_sample sample;

	switch (recode_state) {
	case OFF:
		disable_pmc_on_this_cpu(false);
		goto end;
	case IDLE:
		prev_on = false;
		fallthrough;
	case SYSTEM:
		enable_pmc_on_this_cpu(false);
		break;
	case TUNING:
	case PROFILE:
		/* Toggle PMI */
		if (!curr_on)
			disable_pmc_on_this_cpu(false);
		else {
			pr_info("%u] Enabling PMCs for pid %u\n", cpu,
				current->pid);
			enable_pmc_on_this_cpu(false);
		}
		break;
	default:
		pr_warn("Invalid state on cpu %u... disabling PMCs\n", cpu);
		disable_pmc_on_this_cpu(false);
		goto end;
	}

	// per_cpu(pcpu_pmus_metadata.has_ctx_switch, cpu) = true;

	/* TODO - Think aobut enabling or not the sampling in CTX_SWITCH */

	// if (!prev_on) {
	// 	pmc_generate_snapshot(NULL, false);
	// } else {
	// 	pmc_generate_snapshot(&sample.pmcs, true);
	// 	// TODO - Note here pmcs are active, so we are recording
	// 	//        part of the sample collection work
	// 	sample.id = prev->pid;
	// 	sample.tracked = true;
	// 	sample.k_thread = !prev->mm;
	// 	write_log_sample(per_cpu(pcpu_data_logger, cpu), &sample);
	// }

end:
	put_cpu();
}

void pmi_function(unsigned cpu)
{
	unsigned k;
	struct pmcs_collection *pmcs_collection;
	struct data_collector_sample *dc_sample = NULL;

	if (recode_state == OFF) {
		pr_warn("Recode is OFF - This PMI shouldn't fire, ignoring\n");
		return;
	}

	if (recode_state == IDLE) {
		return;
	}

	atomic_inc(&active_pmis);

	/* pmcs_collection should be corrent as long as it accessed here */
	pmcs_collection = this_cpu_read(pcpu_pmus_metadata.pmcs_collection);

	if (unlikely(!pmcs_collection)) {
		pr_debug("Got a NULL COLLECTION inside PMI\n");
		goto skip;
	}

	if (!per_cpu(pcpu_pmus_metadata.hw_events, cpu)) {
		pr_debug("Got a NULL hw_events inside PMI\n");
		goto skip;
	}

	// /* Get a sample crafted ad-hoc to fit the current hw_events */
	// dc_sample = get_write_dc_sample(
	// 	per_cpu(pcpu_data_collector, cpu),
	// 	per_cpu(pcpu_pmus_metadata.hw_events, cpu)->cnt +
	// 		gbl_nr_pmc_fixed);

	// if (unlikely(!dc_sample)) {
	// 	pr_debug("Got a NULL WR SAMPLE inside PMI\n");
	// 	goto skip;
	// }

	/* Collect the raw pmcs values */
	// memcpy(&dc_sample->pmcs, pmcs_collection,
	//        sizeof(struct pmcs_collection) +
	// 	       (sizeof(pmc_ctr) * pmcs_collection->cnt));

	u64 mask = per_cpu(pcpu_pmus_metadata.hw_events, cpu)->mask;

	dc_sample = get_sample_and_compute_tma(pmcs_collection, mask, cpu);

	/* Get a sample crafted ad-hoc to fit the current hw_events */
	// dc_sample = get_write_dc_sample(
	// 	per_cpu(pcpu_data_collector, cpu),
	// 	per_cpu(pcpu_pmus_metadata.hw_events, cpu)->cnt +
	// 		gbl_nr_pmc_fixed);

	if (unlikely(!dc_sample)) {
		pr_debug("Got a NULL WR SAMPLE inside PMI\n");
		goto skip;
	}

	dc_sample->id = current->pid;
	dc_sample->tracked = true;
	dc_sample->k_thread = !current->mm;

	dc_sample->system_tsc = per_cpu(pcpu_pmus_metadata.last_tsc, cpu);

	dc_sample->tsc_cycles = per_cpu(pcpu_pmus_metadata.sample_tsc, cpu);
	dc_sample->core_cycles = pmcs_collection->pmcs[1];
	dc_sample->core_cycles_tsc_ref = pmcs_collection->pmcs[2];
	// dc_sample->ctx_evts = per_cpu(pcpu_pmus_metadata.ctx_evts, cpu);

	/* get_sample_and_compute_tma calls get_write_dc_sample */
	put_write_dc_sample(per_cpu(pcpu_data_collector, cpu));

	pr_debug("SAMPLE: ");
	for (k = 0; k < dc_sample->pmcs.cnt; ++k) {
		pr_cont("%llu ", pmcs_collection->pmcs[k]);
	}
	// for (k = 0; k < gbl_nr_pmc_fixed; ++k) {
	// 	pr_cont("%llu ", pmcs_collection->pmcs[k]);
	// }
	// pr_cont(" - ");
	// for (k = gbl_nr_pmc_fixed; k < pmcs_collection->cnt; ++k) {
	// 	pr_cont("%llu ", pmcs_collection->pmcs[k]);
	// }
	pr_cont("\n");

	/* Compute TMAM */
	/*
	 * u64 mask = per_cpu(pcpu_pmus_metadata.hw_events, cpu)->mask;
	 * compute_tma(dc_sample, mask);
	 */

	//u64 metrics[] = {L0_BS, L0_FB};
	//u32 metrics_size = 2;
	// u64 mask = per_cpu(pcpu_pmus_metadata.hw_events, cpu)->mask;
	//check_tma(metrics_size, metrics, mask);
	// compute_tma(pmcs_collection, mask, cpu);

	//pr_debug("aaaa\n");

skip:
	atomic_dec(&active_pmis);
}

static void manage_pmu_state(void *dummy)
{
	/* This is not precise */
	this_cpu_write(pcpu_pmus_metadata.last_tsc, rdtsc_ordered());

	on_context_switch_callback(NULL, false, query_tracker(current->pid));
}

void recode_set_state(unsigned state)
{
	enum recode_state old_state = recode_state;
	recode_state = state;

	if (old_state == recode_state)
		return;

	switch (state) {
	case OFF:
		pr_info("Recode state: OFF\n");
		debug_pmu_state();
		disable_pmc_on_system();
		// TODO - Restore
		// flush_written_samples_on_system();
		return;
	case IDLE:
		pr_info("Recode ready for IDLE\n");
		break;
	case PROFILE:
		pr_info("Recode ready for PROFILE\n");
		break;
	case SYSTEM:
		pr_info("Recode ready for SYSTEM\n");
		break;
	default:
		pr_warn("Recode invalid state\n");
		recode_state = old_state;
		return;
	}

	recode_reset_data();
	on_each_cpu(manage_pmu_state, NULL, 0);

	/* Use the cached value */
	/* TODO - check pmc setup methods */
	// setup_pmc_on_system(NULL);
	// TODO - Make this call clear
}

/* Register thread for profiling activity  */
int attach_process(pid_t id)
{
	pr_info("Attaching process: %u\n", id);
	tracker_add(id);
	return 0;
}

/* Remove registered thread from profiling activity */
void detach_process(pid_t id)
{
	pr_info("Detaching process: %u\n", id);
	tracker_del(id);
}

int register_ctx_hook(void)
{
	int err = 0;

	set_hook_callback(&on_context_switch_callback);
	switch_hook_set_state_enable(true);

	return err;
}

void unregister_ctx_hook(void)
{
	set_hook_callback(NULL);
}

void setup_hw_events_from_proc(pmc_evt_code *hw_events_codes, unsigned cnt)
{
	if (!hw_events_codes || !cnt) {
		pr_warn("Invalid hw evenyts codes from user\n");
		return;
	}

	setup_hw_events_on_system(hw_events_codes, cnt);
}
