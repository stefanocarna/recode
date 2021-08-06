#include <asm/apic.h>
#include <asm/perf_event.h>
#include <linux/sched.h>
#include <linux/slab.h>

#include "recode.h"
#include "recode_config.h"
#include "pmu/pmu.h"
#include "pmu/pmi.h"
#include "pmu/pmc_events.h"

pmc_evt_code HW_EVENTS_BITS[] = { { evt_ca_stalls_mem_any },
				  { evt_ca_stalls_total },
				  { evt_ca_stalls_l3_miss },
				  { evt_ca_stalls_l2_miss },
				  { evt_ca_stalls_l1d_miss },
				  { evt_ea_exe_bound_0_ports },
				  { evt_ea_1_ports_util },
				  { evt_ea_2_ports_util },
				  { evt_ea_3_ports_util },
				  { evt_ea_4_ports_util },
				  { evt_ea_bound_on_stores },
				  { evt_rse_empty_cycles },
				  { evt_ur_retire_slots },
				  { evt_cpu_clk_unhalted },
				  { evt_ui_any },
				  { evt_im_recovery_cycles },
				  { evt_iund_core },
				  { stlb_miss_loads },
				  { tlb_page_walk },
				  { evt_loads_all },
				  { evt_stores_all },
				  { evt_l2_reference },
				  { evt_l2_misses },
				  { evt_l2_all_rfo },
				  { evt_l2_rfo_misses },
				  { evt_l2_all_data },
				  { evt_l2_data_misses },
				  { evt_l2_all_code },
				  { evt_l2_code_misses },
				  { evt_l2_all_pre },
				  { evt_l2_pre_misses },
				  { evt_l2_all_demand },
				  { evt_l2_demand_misses },
				  { evt_l2_wb },
				  { evt_l2_in_all },
				  { evt_l2_out_silent },
				  { evt_l2_out_non_silent },
				  { evt_l2_out_useless },
				  { evt_null } };

DEFINE_PER_CPU(struct pmus_metadata, pcpu_pmus_metadata) = { 0 };

unsigned __read_mostly gbl_nr_pmc_fixed = 0;
unsigned __read_mostly gbl_nr_pmc_general = 0;

u64 active_mask = 0;

void get_machine_configuration(void)
{
	union cpuid10_edx edx;
	union cpuid10_eax eax;
	union cpuid10_ebx ebx;
	unsigned int unused;
	unsigned version;

	cpuid(10, &eax.full, &ebx.full, &unused, &edx.full);

	if (eax.split.mask_length < 7)
		return;

	version = eax.split.version_id;
	gbl_nr_pmc_general = eax.split.num_counters;
	gbl_nr_pmc_fixed = edx.split.num_counters_fixed;

	pr_info("PMU Version: %u\n", version);
	pr_info(" - NR Counters: %u\n", eax.split.num_counters);
	pr_info(" - Counter's Bits: %u\n", eax.split.bit_width);
	pr_info(" - Counter's Mask: %llx\n", (1ULL << eax.split.bit_width) - 1);
	pr_info(" - NR PEBS' events: %x\n",
		min_t(unsigned, 8, eax.split.num_counters));
}

void fast_setup_general_pmc_on_cpu(unsigned cpu, struct pmc_evt_sel *pmc_cfgs,
				   unsigned off, unsigned cnt)
{
	u64 ctrl;
	unsigned pmc;

	/* Avoid to write wrong MSRs */
	if (cnt > gbl_nr_pmc_general)
		cnt = gbl_nr_pmc_general;

	ctrl = FIXED_TO_BITS_MASK | (BIT_ULL(cnt) - 1);

	/* Uneeded PMCs are disabled in ctrl */
	for_each_active_general_pmc (ctrl, pmc) {
		SETUP_GENERAL_PMC(pmc, pmc_cfgs[pmc + off].perf_evt_sel);
		WRITE_GENERAL_PMC(pmc, 0ULL);
	}

	per_cpu(pcpu_pmus_metadata.perf_global_ctrl, cpu) = ctrl;
}

void read_all_pmcs(struct pmcs_snapshot *snapshot)
{
	/* TODO - Check */

	// unsigned pmc;
	// if (!snapshot) {
	// 	pr_warn("Cannot save PMCs on NULL snapshot\n");
	// 	return;
	// }

	// snapshot->tsc = (u64)rdtsc_ordered();

	// /* Read all active fixed pmcs */
	// for_each_fixed_pmc (pmc) {
	// 	if (perf_global_ctrl &
	// 	    BIT_ULL(pmc + PERF_GLOBAL_CTRL_FIXED0_SHIFT)) {
	// 		snapshot->fixed[pmc] = READ_FIXED_PMC(pmc);
	// 	}
	// }
	// /* Read all active general pmcs */
	// for_each_general_pmc (pmc) {
	// 	if (perf_global_ctrl & BIT_ULL(pmc)) {
	// 		snapshot->general[pmc] = READ_GENERAL_PMC(pmc);
	// 	}
	// }
}

static void __enable_pmc_on_cpu(void *dummy)
{
	// TODO - Remove
	if (smp_processor_id() != 3)
		return;

	if (recode_state == OFF) {
		pr_warn("Cannot enable pmc on cpu %u while Recode is OFF\n",
			smp_processor_id());
		return;
	}
	this_cpu_write(pcpu_pmus_metadata.pmcs_active, true);
#ifndef CONFIG_RUNNING_ON_VM
	wrmsrl(MSR_CORE_PERF_GLOBAL_CTRL,
	       this_cpu_read(pcpu_pmus_metadata.perf_global_ctrl));
	pr_debug("enabled pmcs on cpu %u\n", smp_processor_id());
#endif
}

static void __disable_pmc_on_cpu(void *dummy)
{
	this_cpu_write(pcpu_pmus_metadata.pmcs_active, false);
#ifndef CONFIG_RUNNING_ON_VM
	wrmsrl(MSR_CORE_PERF_GLOBAL_CTRL, 0ULL);
	pr_debug("disabled pmcs on cpu %u\n", smp_processor_id());
#endif
}

void inline __attribute__((always_inline)) enable_pmc_on_this_cpu(bool force)
{
	if (force || !this_cpu_read(pcpu_pmus_metadata.pmcs_active))
		__enable_pmc_on_cpu(NULL);
}

void inline __attribute__((always_inline)) disable_pmc_on_this_cpu(bool force)
{
	if (force || this_cpu_read(pcpu_pmus_metadata.pmcs_active))
		__disable_pmc_on_cpu(NULL);
}

static void __restore_and_enable_pmc_on_this_cpu(bool *state)
{
	if (*state)
		enable_pmc_on_this_cpu(true);
}

static void __save_and_disable_pmc_on_this_cpu(bool *state)
{
	*state = this_cpu_read(pcpu_pmus_metadata.pmcs_active);
	disable_pmc_on_this_cpu(true);
}

void inline __attribute__((always_inline)) enable_pmc_on_system(void)
{
	on_each_cpu(__enable_pmc_on_cpu, NULL, 1);
}

void inline __attribute__((always_inline)) disable_pmc_on_system(void)
{
	on_each_cpu(__disable_pmc_on_cpu, NULL, 1);
}

struct pmcs_collection *get_pmcs_collection_on_this_cpu(void)
{
	struct pmcs_collection *pmcs_collection =
		this_cpu_read(pcpu_pmus_metadata.pmcs_collection);

	if (!pmcs_collection || !pmcs_collection->complete)
		return NULL;

	return pmcs_collection;
}

void debug_pmu_state(void)
{
	u64 msr;
	unsigned cpu = get_cpu();

	pr_info("Init PMU debug on core %u\n", cpu);

	rdmsrl(MSR_CORE_PERF_GLOBAL_STATUS, msr);
	pr_info("MSR_CORE_PERF_GLOBAL_STATUS: %llx\n", msr);

	rdmsrl(MSR_CORE_PERF_FIXED_CTR_CTRL, msr);
	pr_info("MSR_CORE_PERF_FIXED_CTR_CTRL: %llx\n", msr);

	pr_info("GP 0: %llx\n", QUERY_GENERAL_PMC(0));
	pr_info("GP 1: %llx\n", QUERY_GENERAL_PMC(1));
	pr_info("GP 2: %llx\n", QUERY_GENERAL_PMC(2));
	pr_info("GP 3: %llx\n", QUERY_GENERAL_PMC(3));

	pr_info("Fini PMU debug on core %u\n", cpu);

	put_cpu();
}

static void flush_and_clean_hw_events(void)
{
}

/* Required preemption disabled */
void setup_hw_events_on_cpu(void *hw_events)
{
	bool state;
	unsigned cnt;
	struct pmcs_collection *pmcs_collection;

	__save_and_disable_pmc_on_this_cpu(&state);

	flush_and_clean_hw_events();

	if (!hw_events) {
		pr_warn("Cannot setup hw_events on cpu %u: hw_events is NULL\n",
			smp_processor_id());
		goto end;
	}

	cnt = ((struct hw_events *)hw_events)->cnt;

	pmcs_collection = this_cpu_read(pcpu_pmus_metadata.pmcs_collection);

	/* Free old values */
	if (pmcs_collection && pmcs_collection->cnt >= cnt)
		goto skip_alloc;

	if (pmcs_collection)
		kfree(pmcs_collection);

	pmcs_collection =
		kzalloc(sizeof(struct pmcs_collection) +
				(sizeof(pmc_ctr) * (cnt + gbl_nr_pmc_fixed)),
			GFP_KERNEL);

	if (!pmcs_collection) {
		pr_warn("Cannot allocate memory for pmcs_collection on cpu %u\n",
			smp_processor_id());
		goto end;
	}

	pmcs_collection->complete = false;
	pmcs_collection->cnt = cnt + gbl_nr_pmc_fixed;
	pmcs_collection->mask = ((struct hw_events *)hw_events)->mask;

	/* Update the new pmcs_collection value */
	this_cpu_write(pcpu_pmus_metadata.pmcs_collection, pmcs_collection);

skip_alloc:
	/* Update hw_events */
	this_cpu_write(pcpu_pmus_metadata.hw_events_index, 0);
	this_cpu_write(pcpu_pmus_metadata.hw_events, hw_events);

	fast_setup_general_pmc_on_cpu(smp_processor_id(),
				      ((struct hw_events *)hw_events)->cfgs, 0,
				      cnt);

end:
	__restore_and_enable_pmc_on_this_cpu(&state);
}

/* TODO - When the hw_events is configured the old hw_events is not freed */
int setup_hw_events_on_system(pmc_evt_code *hw_events_codes, unsigned cnt)
{
	u64 mask;
	unsigned b, i;
	struct hw_events *hw_events;

	pr_debug("Expected %u he_events_codes\n", cnt);

	if (!hw_events_codes || !cnt) {
		pr_warn("Invalid hw_events. Cannot proceed with setup\n");
		return -EINVAL;
	}

	mask = compute_hw_events_mask(hw_events_codes, cnt);

	/* Check if the mask is already registered */
	if (active_mask == mask) {
		pr_debug("Mask %llx is already used. Skip\n", mask);
		return 0;
	}

	active_mask = mask;

	hw_events = kzalloc(sizeof(struct hw_events) +
				    (sizeof(struct pmc_evt_sel) * cnt),
			    GFP_KERNEL);

	if (!hw_events) {
		pr_warn("Cannot allocate memory for hw_events\n");
		return -ENOMEM;
	}


	/* Remove duplicates */
	for (i = 0, b = 0; b < 64; ++b) {
		if (!(mask & BIT(b)))
			continue;

		hw_events->cfgs[i].perf_evt_sel = HW_EVENTS_BITS[b].raw;
		/* PMC setup */
		hw_events->cfgs[i].usr = !!(params_cpl_usr);
		hw_events->cfgs[i].os = !!(params_cpl_os);
		hw_events->cfgs[i].pmi = 0;
		hw_events->cfgs[i].en = 1;
		pr_debug("Configure HW_EVENT %llx\n",
			 hw_events->cfgs[i].perf_evt_sel);
		++i;
	}

	hw_events->cnt = i;
	hw_events->mask = mask;

	// TODO restore
	on_each_cpu(setup_hw_events_on_cpu, hw_events, 1);

	pr_info("HW_MASK: %llx\n", hw_events->mask);

	return 0;
}

static void __init_pmu_on_cpu(void *dummy)
{
#ifndef CONFIG_RUNNING_ON_VM
	u64 msr;
	unsigned pmc;

	/* Refresh APIC entry */
	if (recode_pmi_vector == NMI)
		apic_write(APIC_LVTPC, LVT_NMI);
	else
		apic_write(APIC_LVTPC, RECODE_PMI);

	/* Clear a possible stale state */
	rdmsrl(MSR_CORE_PERF_GLOBAL_STATUS, msr);
	wrmsrl(MSR_CORE_PERF_GLOBAL_OVF_CTRL, msr);

	/* Enable FREEZE_ON_PMI */
	wrmsrl(MSR_IA32_DEBUGCTLMSR, BIT(12));

	/* TODO - At the moment both fixed_ctrl and gbl_fixed_pmc_pmi are global */
	for_each_fixed_pmc (pmc) {
		if (pmc == gbl_fixed_pmc_pmi) {
			WRITE_FIXED_PMC(pmc, gbl_reset_period);
		} else {
			WRITE_FIXED_PMC(pmc, 0ULL);
		}
	}

	/* Setup FIXED PMCs */
	wrmsrl(MSR_CORE_PERF_FIXED_CTR_CTRL,
	       this_cpu_read(pcpu_pmus_metadata.fixed_ctrl));

	/* TODO - MAke it compliant to pcpu_pmus_metadata.fixed_ctrl */
	this_cpu_write(pcpu_pmus_metadata.perf_global_ctrl, FIXED_TO_BITS_MASK);

#else
	pr_warn("CONFIG_RUNNING_ON_VM is enabled. PMCs are ignored\n");
#endif
}

int init_pmu_on_system(void)
{
	unsigned cpu, pmc;
	u64 gbl_fixed_ctrl = 0;
	/* Compute fixed_ctrl */

	for_each_fixed_pmc (pmc) {
		/**
		 * bits: 3   2   1   0
		 * 	PMI, 0, USR, OS
		 */
		if (pmc == gbl_fixed_pmc_pmi) {
			/* Set PMI */
			gbl_fixed_ctrl |= (BIT(3) << (pmc * 4));
			gbl_fixed_ctrl |= (BIT(0) << (pmc * 4));
		}
		if (params_cpl_usr)
			gbl_fixed_ctrl |= (BIT(1) << (pmc * 4));
		if (params_cpl_os)
			gbl_fixed_ctrl |= (BIT(0) << (pmc * 4));
	}

	for_each_online_cpu (cpu) {
		per_cpu(pcpu_pmus_metadata.fixed_ctrl, cpu) = gbl_fixed_ctrl;
	}

	/* Metadata doesn't require initialization at the moment */
	on_each_cpu(__init_pmu_on_cpu, NULL, 1);

	pr_warn("PMUs initialized on all cores\n");
	return 0;
}

void cleanup_pmc_on_system(void)
{
	/* TODO - To be implemented */
	on_each_cpu(__disable_pmc_on_cpu, NULL, 1);
}

u64 compute_hw_events_mask(pmc_evt_code *hw_events_codes, unsigned cnt)
{
	u64 mask = 0;
	unsigned evt, bit;

	for (evt = 0; evt < cnt; ++evt) {
		bit = 0;
		while (HW_EVENTS_BITS[bit].raw) {
			/* TODO - Convert into bitmap */
			if (hw_events_codes[evt].raw ==
			    HW_EVENTS_BITS[bit].raw) {
				pr_debug("Found HW_EVT %x\n",
					 hw_events_codes[evt].raw);
				mask |= BIT(bit);
				break;
			}
			bit++;
		}
	}

	return mask;
}