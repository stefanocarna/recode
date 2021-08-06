
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
	
	while(--cpu)
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

	// if (recode_state == OFF) {
	// 	pr_warn("Recode is OFF - This PMI shouldn't fire, ignoring\n");
	// 	return;
	// }

	// // TODO - it may require the active_pmis signals
	// if (recode_state == IDLE) {
	// 	pmc_generate_snapshot(&sample.pmcs, false);
	// 	return;
	// }

	// if (atomic_read(&on_samples_flushing)) {
	// 	pr_debug("Skipping pmi_function because on_samples_flushing\n");
	// 	return;
	// }

	atomic_inc(&active_pmis);

	// pmc_generate_snapshot(&sample.pmcs, false);

	pmcs_collection = get_pmcs_collection_on_this_cpu();

	if (unlikely(!pmcs_collection)) {
		pr_debug("Got a NULL COLLECTION inside PMI\n");
		goto skip;
	}

	if (!per_cpu(pcpu_pmus_metadata.hw_events, cpu)) {
		pr_debug("Got a NULL hw_events inside PMI\n");
		goto skip;
	}

	dc_sample = get_write_dc_sample(
		per_cpu(pcpu_data_collector, cpu),
		per_cpu(pcpu_pmus_metadata.hw_events, cpu)->cnt +
			gbl_nr_pmc_fixed);

	if (unlikely(!dc_sample)) {
		pr_debug("Got a NULL WR SAMPLE inside PMI\n");
		goto skip;
	}

	memcpy(dc_sample->pmcs.pmcs, pmcs_collection,
	       sizeof(struct pmcs_collection) +
		       (sizeof(pmc_ctr) * pmcs_collection->cnt));


	pr_debug("SAMPLE: ");
	for (k = 0; k < pmcs_collection->cnt; ++k) {
		pr_cont("%llx ", pmcs_collection->pmcs[k]);
	}
	pr_cont("\n");

	put_write_dc_sample(per_cpu(pcpu_data_collector, cpu));
	// write_log_sample(per_cpu(pcpu_data_logger, cpu), pmcs_collection);

	// sample.id = current->pid;
	// sample.tracked = true;
	// sample.k_thread = !current->mm;
	// write_log_sample(per_cpu(pcpu_data_logger, cpu), &sample);

skip:
	atomic_dec(&active_pmis);
}

void pmc_generate_snapshot(struct pmcs_snapshot *sample_pmcs, bool pmc_off)
{
	unsigned k;
	pmc_ctr old_pmc_pmi, new_pmc_pmi;
	unsigned cpu = get_cpu();
	bool overflow = !pmc_off;
	/* At this moment @new_pmcs refers to the last snapshot */
	struct pmcs_snapshot *new_pmcs = per_cpu_ptr(&pcpu_pmcs_snapshot, cpu);

	if (pmc_off)
		disable_pmc_on_this_cpu(false);

	if (sample_pmcs)
		/* At this moment @sample_pmcs refers to the last snapshot */
		memcpy(sample_pmcs, new_pmcs, sizeof(struct pmcs_snapshot));

	/* Read current PMCs' value and place into @new_pmcs */
	read_all_pmcs(new_pmcs);

	if (sample_pmcs) {
		old_pmc_pmi = sample_pmcs->fixed[gbl_fixed_pmc_pmi];
		new_pmc_pmi = new_pmcs->fixed[gbl_fixed_pmc_pmi];

		/* Compute the sample values and place into @sample_pmcs */
		for_each_pmc (k, gbl_nr_pmc_fixed + gbl_nr_pmc_general) {
			sample_pmcs->pmcs[k] = PMC_TRIM(new_pmcs->pmcs[k] -
							sample_pmcs->pmcs[k]);
		}

		/* TSC is not computed and it is set to "this" moment */
		sample_pmcs->tsc = new_pmcs->tsc;

		/**
		 * So far we have that
		 * @new_pmcs 	: holds the last pmcs_snapshot (raw values)
		 * @sample_pmcs : holds the last pmcs_sample (computed values)
		 * @old_pmc_pmi : holds the old raw value for the pmc_pmi
		 * @new_pmc_pmi : holds the new raw value for the pmc_pmi
		 */

		if (overflow) { // sample_pmcs->fixed[gbl_fixed_pmc_pmi] == 0
			// new_pmc_pmi == 0x2c
			/* We got no CTX switches in the meanwhile */
			if (old_pmc_pmi < PMI_DELAY) {
				sample_pmcs->fixed[gbl_fixed_pmc_pmi] =
					PMC_TRIM(PMC_CTR_MAX -
						 gbl_reset_period);
			} else {
				sample_pmcs->fixed[gbl_fixed_pmc_pmi] =
					PMC_TRIM(PMC_CTR_MAX - old_pmc_pmi);
			}
		} else {
			if (old_pmc_pmi < PMI_DELAY)
				sample_pmcs->fixed[gbl_fixed_pmc_pmi] =
					PMC_TRIM(new_pmc_pmi -
						 gbl_reset_period);
			else
				sample_pmcs->fixed[gbl_fixed_pmc_pmi] =
					PMC_TRIM(new_pmc_pmi - old_pmc_pmi);
		}

		// TODO - Delete or improve
		if (cpu == 0) {
			pr_debug("[CTX %u] OLD: %llx, NEW: %llx, DIFF %llx\n",
				 pmc_off, old_pmc_pmi, new_pmc_pmi,
				 sample_pmcs->fixed[gbl_fixed_pmc_pmi]);
		}
	}

	if (pmc_off)
		enable_pmc_on_this_cpu(false);

	put_cpu();
}

static void manage_pmu_state(void *dummy)
{
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