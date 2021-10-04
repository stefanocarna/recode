#include <linux/slab.h>
#include <linux/dynamic-mitigations.h>

#include "dependencies.h"

#include "pmu/hw_events.h"
#include "recode_security.h"

#define STATS_EN_MASK BIT(31)
#define MSK_16(v) (v & (BIT(16) - 1))

#define NR_THRESHOLDS 5
/* The last value is the number of samples while tuning the system */
s64 thresholds[NR_THRESHOLDS + 1] = { 950, 950, 0, 0, 950, 0 };

#define DEFAULT_TS_PRECISION 1000

atomic_t detected_theads;

unsigned ts_precision = DEFAULT_TS_PRECISION;
unsigned ts_precision_5 = (DEFAULT_TS_PRECISION * 0.05);
unsigned ts_window = 5;
unsigned ts_alpha = 1;
unsigned ts_beta = 1;

int recode_security_init(void)
{
#define NR_SC_HW_EVTS 6

	unsigned k = 0;

	pmc_evt_code *SC_HW_EVTS =
		kmalloc(sizeof(pmc_evt_code *) * NR_SC_HW_EVTS, GFP_KERNEL);

	if (!SC_HW_EVTS) {
		pr_err("Cannot allocate memory for SC_HW_EVTS\n");
		return -ENOMEM;
	}

	SC_HW_EVTS[k++].raw = HW_EVT_COD(l2_all_data);
	SC_HW_EVTS[k++].raw = HW_EVT_COD(l2_data_misses);
	SC_HW_EVTS[k++].raw = HW_EVT_COD(l3_miss_data);
	SC_HW_EVTS[k++].raw = HW_EVT_COD(l2_wb);
	SC_HW_EVTS[k++].raw = HW_EVT_COD(l2_in_all);
	SC_HW_EVTS[k++].raw = HW_EVT_COD(tlb_page_walk);

	recode_callbacks.on_pmi = on_pmi;
	recode_callbacks.on_ctx = on_ctx;
	recode_callbacks.on_state_change = on_state_change;

	setup_hw_events_on_system(SC_HW_EVTS, NR_SC_HW_EVTS);

	return 0;
}

void recode_security_fini(void)
{
}

static void tuning_finish_callback(void *dummy)
{
	unsigned k;

	recode_set_state(OFF);

	pr_warn("Tuning finished\n");
	pr_warn("Got %llu samples\n", thresholds[NR_THRESHOLDS]);

	for (k = 0; k < NR_THRESHOLDS; ++k) {
		u64 backup = thresholds[k];
		thresholds[k] /= (thresholds[NR_THRESHOLDS] + 1);
		pr_warn("TS[%u]: %lld/%u (%llu)\n", k, thresholds[k],
			ts_precision, backup);
	}

	set_exit_callback(NULL);
	pr_warn("Tuning finished\n");
}

bool on_state_change(enum recode_state state)
{
	if (state == TUNING) {
		unsigned k = NR_THRESHOLDS + 1;
		while (k--) {
			thresholds[k] = 0;
		}
		set_exit_callback(tuning_finish_callback);
		pr_warn("Recode ready for TUNING\n");
	} else {
		set_exit_callback(NULL);
	}

	return false;
}

void print_spy(char *s)
{
	if (current->comm[0] == 's' && current->comm[1] == 'p' &&
	    current->comm[2] == 'y') {
		pr_info("[%u] SPY ON %s\n", current->pid, s);
	}
}

void on_ctx(struct task_struct *prev, bool prev_on, bool curr_on)
{
	unsigned cpu = get_cpu();

	if (!prev_on)
		return;

	/* Disable PMUs, hence PMI */
	disable_pmc_on_this_cpu(false);

	// We may skip sample at ctx
	// if (pmc_generate_collection(cpu))
	// 	pmc_evaluate_activity(
	// 		prev,
	// 		this_cpu_read(pcpu_pmus_metadata.pmcs_collection));

	// CLEAN PMCs
	pmc_generate_collection(cpu);

	/* Enable PMUs */
	enable_pmc_on_this_cpu(false);

	/* Activate on previous task */
	if (has_pending_mitigations(prev))
		enable_mitigations_on_task(prev);

	/* Enable mitigations */
	mitigations_switch(prev, current);
	LLC_flush(current);

	put_cpu();
}

static void enable_detection_statistics(struct task_struct *tsk)
{
	struct detect_stats *ds;

	/* Skif if already allocated */
	if (tsk->monitor_state & STATS_EN_MASK)
		return;

	ds = kmalloc(sizeof(struct detect_stats), GFP_ATOMIC);

	if (!ds) {
		pr_warn("@%u] Cannot allocate statistics for pid %u\n",
			smp_processor_id(), tsk->pid);
		return;
	}

	/* Copy and safe truncate the task name */
	memcpy(ds->comm, tsk->comm, sizeof(ds->comm));
	ds->comm[sizeof(ds->comm) - 1] = '\0';

	ds->pid = tsk->pid;
	ds->tgid = tsk->tgid;

	ds->nvcsw = tsk->nvcsw;
	ds->nivcsw = tsk->nivcsw;
	ds->pmis = 0;
	ds->skpmis = 0;
	ds->utime = tsk->utime;
	ds->stime = tsk->stime;

	/* Attach stats to task */
	tsk->monitor_state |= STATS_EN_MASK;
	tsk->monitor_data = (void *)ds;
}

static void kill_detected_process(struct task_struct *tsk)
{
	// int ret = send_sig_info(SIGSEGV, SEND_SIG_NOINFO , tsk);
	// if (ret < 0) {
	// 	pr_warn("Error sending signal to process: %u\n", tsk->pid);
	// }
}

static void finalize_detection_statisitcs(struct task_struct *tsk)
{
	struct detect_stats *ds;

	/* Skip KThreads */
	if (!tsk->mm)
		return;

	/* Something went wrong */
	if (!(tsk->monitor_state & STATS_EN_MASK))
		return;

	ds = (struct detect_stats *)tsk->monitor_data;

	ds->nvcsw = tsk->nvcsw - ds->nvcsw;
	ds->nivcsw = tsk->nivcsw - ds->nivcsw;
	ds->utime = tsk->utime - ds->utime;
	ds->stime = tsk->stime - ds->stime;

	tsk->monitor_state &= ~STATS_EN_MASK;
	tsk->monitor_data = NULL;

	/* Print all the values */
	pr_warn("DETECTED pid %u (tgid %u): %s\n", ds->pid, ds->tgid, ds->comm);
	printk(" **   VCSW --> %u\n", ds->nvcsw);
	printk(" **  IVCSW --> %u\n", ds->nivcsw);
	printk(" **  UTIME --> %llu\n", ds->utime);
	printk(" **  STIME --> %llu\n", ds->stime);
	printk(" **   PMIs --> %u\n", ds->pmis);
	printk(" ** skPMIs --> %u\n", ds->skpmis);
	printk(" ****  END OF REPORT  ****\n");
	kfree(ds);
}

static bool evaluate_pmcs(struct task_struct *tsk,
			  struct pmcs_collection *snapshot)
{
	/* Skip KThreads */
	if (!tsk->mm)
		goto end;

	if (recode_state == IDLE) {
		/* Do nothing */
	} else if (recode_state == TUNING) {
		thresholds[0] += DM0(ts_precision, snapshot);
		thresholds[1] += DM1(ts_precision, snapshot);
		thresholds[2] += DM2(ts_precision, snapshot);
		thresholds[3] += DM3(ts_precision, snapshot);
		// thresholds[4] += DM4(ts_precision, snapshot);
		thresholds[NR_THRESHOLDS]++;

		// pr_warn("Got sample %llu\n", thresholds[0]);
	} else {
		if (!has_mitigations(tsk)) {
			// TODO Remove - Init task_struct
			if (MSK_16(tsk->monitor_state) > ts_window + 1)
				tsk->monitor_state &= ~(BIT(16) - 1);

			if ((CHECK_LESS_THAN_TS(thresholds[0],
						DM0(ts_precision, snapshot),
						ts_precision_5) &&
			     CHECK_LESS_THAN_TS(thresholds[1],
						DM1(ts_precision, snapshot),
						ts_precision_5) &&
			     CHECK_MORE_THAN_TS(thresholds[2],
						DM2(ts_precision, snapshot),
						ts_precision_5) &&
			     CHECK_MORE_THAN_TS(thresholds[3],
						DM3(ts_precision, snapshot),
						ts_precision_5)) ||
			    /* P4 */
			    CHECK_LESS_THAN_TS(thresholds[4],
					       DM3(ts_precision, snapshot),
					       ts_precision_5)) {
				tsk->monitor_state += ts_alpha;
				pr_info("[++] %s (PID %u): %x\n", tsk->comm,
					tsk->pid, tsk->monitor_state);
				/* Enable statitics for this task */
				enable_detection_statistics(tsk);
			} else if (MSK_16(tsk->monitor_state) > 0) {
				tsk->monitor_state -= ts_beta;
				pr_info("[--] %s (PID %u): %x\n", tsk->comm,
					tsk->pid, tsk->monitor_state);
			}

			if (MSK_16(tsk->monitor_state) > ts_window) {
				pr_warn("[FLAG] Detected %s (PID %u): %u\n",
					tsk->comm, tsk->pid,
					tsk->monitor_state);

				/* Close statitics for this task */
				atomic_inc(&detected_theads);
				finalize_detection_statisitcs(tsk);
				kill_detected_process(tsk);
				return true;
			}
		}
	}
end:
	return false;
}

void pmc_evaluate_activity(unsigned cpu, struct task_struct *tsk,
			   struct pmcs_collection *pmcs)
{
	// get_cpu();

	if (unlikely(!pmcs)) {
		pr_warn("Called pmc_evaluate_activity without pmcs... skip");
		goto end;
	}

	if (evaluate_pmcs(tsk, pmcs)) {
		/* Delay activation if we are inside the PMI */
		request_mitigations_on_task(tsk, true);
	}

end:
	return;
	// put_cpu();
}

void on_pmi(unsigned cpu, struct pmus_metadata *pmus_metadata)
{
	pmc_evaluate_activity(cpu, current, pmus_metadata->pmcs_collection);
}