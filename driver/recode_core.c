
#include <asm/msr.h>
#include <linux/sched.h>
#include <linux/pmc_dynamic.h>
#include <linux/percpu-defs.h>

#include "dependencies.h"
#include "device/proc.h"
#include "recode_core.h"

#define BUFF_SIZE	PAGE_SIZE * 48
#define BUFF_LENGTH	((BUFF_SIZE) / sizeof(struct pmc_snapshot))

#define TAKE_FX_SNAPSHOT(pmc)	this_cpu_write(pcpu_pmc_snapshot_out.pmc, ctx_snap->pmc);
#define COMPUTE_DELTA(pmc)	delta_snap->pmc = ctx_snap->pmc - this_cpu_read(pcpu_pmc_snapshot_out.pmc)
#define ADD_DELTA(pmc)		prev->pmc_user->pmc += delta_snap->pmc
#define COMPUTE_DATA(pmc)	prev->pmc_user->pmc += ctx_snap->pmc - this_cpu_read(pcpu_pmc_snapshot_out.pmc)

#define FULL_PMC_ACTION(a)	a(fixed0); 	\
				a(fixed1); 	\
				a(fixed2); 	\
				a(pmc0);  	\
				a(pmc1);  	\
				a(pmc2);  	\
				a(pmc3);  	\
				a(pmc4);  	\
				a(pmc5);  	\
				a(pmc6);  	\
				a(pmc7)  	
				
DEFINE_PER_CPU(struct pmc_snapshot, pcpu_pmc_snapshot_ctx);
DEFINE_PER_CPU(struct pmc_logger *, pcpu_pmc_logger);

int recode_data_init(void)
{
	unsigned cpu;

	for_each_online_cpu(cpu) {
		per_cpu(pcpu_pmc_logger, cpu) = vmalloc(sizeof(struct pmc_logger) + (BUFF_LENGTH * sizeof(struct pmc_snapshot)));
		if (!per_cpu(pcpu_pmc_logger, cpu)) 
			goto mem_err;
		per_cpu(pcpu_pmc_logger, cpu)->idx = 0;
		per_cpu(pcpu_pmc_logger, cpu)->length = BUFF_LENGTH;
		per_cpu(pcpu_pmc_logger, cpu)->cpu = cpu;
	}

	return 0;
mem_err:
	pr_info("failed to allocate percpu pcpu_pmc_buffer\n");
	return -1;
}

void recode_data_fini(void)
{
	unsigned cpu;

	for_each_online_cpu(cpu)
		vfree(per_cpu(pcpu_pmc_logger, cpu));
}


static void pcpu_disable_pmc(void *dummy)
{
	wrmsrl(MSR_CORE_PERF_GLOBAL_CTRL, 0ULL);
}

static void pcpu_enable_pmc(void *dummy)
{
	wrmsrl(MSR_CORE_PERF_GLOBAL_CTRL, 0xFFULL | BIT(32) | BIT(33) | BIT(34));
}

static void pcpu_setup_pmc(void *new_cfgs)
{
	unsigned k;
	struct pmc_cfg *cfgs = (struct pmc_cfg *) new_cfgs;

	wrmsrl(MSR_CORE_PERF_FIXED_CTR_CTRL, 0x333);

	for (k = 0; k < MAX_ID_PMC; ++k) {
		wrmsrl(MSR_CORE_PERFEVTSEL(k), cfgs[k].perf_evt_sel);
		pd_write_pmc(k, 0ULL);
	}
}



// ['inst_retired.any', 'cycles', 'cycle_activity.stalls_mem_any', 'cycle_activity.stalls_total',
//               'cycle_activity.stalls_l3_miss', 'exe_activity.bound_on_stores',
//               'cycle_activity.stalls_l1d_miss', 'cycle_activity.stalls_l2_miss']


// t1_event_list_ext = ['uops_retired.retire_slots', 'idq_uops_not_delivered.core',
//                      'uops_issued.any', 'int_misc.recovery_cycles_any']


#define evt_umask_cmask(e, u, c)	(0ULL | e | (u << 8) | (c << 24))

#define evt_ca_stalls_mem_any		evt_umask_cmask(0xa3, 0x10, 16)
#define evt_ca_stalls_total		evt_umask_cmask(0xa3, 0x04, 4)
#define evt_ca_stalls_l3_miss		evt_umask_cmask(0xa3, 0x06, 6)
#define evt_ca_stalls_l2_miss		evt_umask_cmask(0xa3, 0x05, 5)
#define evt_ca_stalls_l1d_miss		evt_umask_cmask(0xa3, 0x0c, 12)
#define evt_ea_bound_on_stores		evt_umask_cmask(0xa6, 0x40, 0)

#define evt_ur_retire_slots		evt_umask_cmask(0xc2, 0x02, 0)
#define evt_iund_core			evt_umask_cmask(0x9c, 0x01, 0)
#define evt_ui_any			evt_umask_cmask(0x0e, 0x01, 0)
#define evt_im_recovery_cycles_any	evt_umask_cmask(0x0d, 0x01, 0)

// Level 2
#define evt_loads_all			evt_umask_cmask(0xd0, 0x81, 0)
#define evt_stores_all			evt_umask_cmask(0xd0, 0x82, 0)

// Not used
#define evt_l1_hit			evt_umask_cmask(0xd1, 0x01, 0)

#define evt_l2_hit			evt_umask_cmask(0xd1, 0x02, 0)
#define evt_l2_miss			evt_umask_cmask(0xd1, 0x10, 0)

#define evt_l3_hit			evt_umask_cmask(0xd1, 0x04, 0)
#define evt_l3_miss			evt_umask_cmask(0xd1, 0x20, 0)

#define evt_l3h_xsnp_miss		evt_umask_cmask(0xd2, 0x01, 0)
#define evt_l3h_xsnp_hit		evt_umask_cmask(0xd2, 0x02, 0)
#define evt_l3h_xsnp_hitm		evt_umask_cmask(0xd2, 0x04, 0)
#define evt_l3h_xsnp_none		evt_umask_cmask(0xd2, 0x08, 0)

#define evt_llc_reference		evt_umask_cmask(0x2e, 0x4f, 0)
#define evt_llc_misses			evt_umask_cmask(0x2e, 0x41, 0)

/* L2 Events */

#define evt_l2_reference		evt_umask_cmask(0x24, 0xef, 0)	// Undercounts
#define evt_l2_misses			evt_umask_cmask(0x24, 0x3f, 0)	

#define evt_l2_all_rfo			evt_umask_cmask(0x24, 0xe2, 0)
#define evt_l2_rfo_misses		evt_umask_cmask(0x24, 0x22, 0)

#define evt_l2_all_data			evt_umask_cmask(0x24, 0xe1, 0)
#define evt_l2_data_misses		evt_umask_cmask(0x24, 0x21, 0)

#define evt_l2_all_code			evt_umask_cmask(0x24, 0xe4, 0)
#define evt_l2_code_misses		evt_umask_cmask(0x24, 0x24, 0)

#define evt_l2_all_pre			evt_umask_cmask(0x24, 0xf8, 0)
#define evt_l2_pre_misses		evt_umask_cmask(0x24, 0x38, 0)

#define evt_l2_all_demand		evt_umask_cmask(0x24, 0xe7, 0)
#define evt_l2_demand_misses		evt_umask_cmask(0x24, 0x27, 0)

#define evt_l2_wb			evt_umask_cmask(0xf0, 0x40, 0)
#define evt_l2_in_all			evt_umask_cmask(0xf1, 0x07, 0)

#define evt_l2_out_silent		evt_umask_cmask(0xf2, 0x01, 0)
#define evt_l2_out_non_silent		evt_umask_cmask(0xf2, 0x02, 0)
#define evt_l2_out_useless		evt_umask_cmask(0xf2, 0x04, 0)


#define LEVEL1

#ifdef LEVEL1
static u64 pmc_events[MAX_ID_PMC] = {
	evt_ur_retire_slots,
	evt_iund_core,
	// evt_ui_any,			// Bad_Spec
	// evt_im_recovery_cycles_any,  // Bad Spec
	evt_ca_stalls_total,		// Spot
	evt_ca_stalls_l3_miss,		// L3 and DRAM Bound
	evt_ca_stalls_mem_any,
	evt_ea_bound_on_stores,
	evt_ca_stalls_l2_miss,
	evt_ca_stalls_l1d_miss
};
#endif
#ifdef LEVEL2
static u64 pmc_events[MAX_ID_PMC] = {
	// evt_stores_all,
	// evt_loads_all,
	// evt_l3_miss,			// L3 and DRAM Bound
	// evt_l1_hit,
	// evt_l2_hit,
	// evt_l3_hit,
	// evt_l3_miss,			// L3 and DRAM Bound
	// evt_l3h_xsnp_hit,
	// evt_l3h_xsnp_miss,

	// evt_l3h_xsnp_hitm,		// Spot

	// evt_llc_reference,
	// evt_llc_misses,

	// evt_l2_reference,
	// evt_l2_all_demand,
	// evt_l2_all_data,
	// evt_l2_all_code,
	// evt_l2_all_rfo,
	// evt_l2_all_pre,

	/* MISSES */
	// evt_l2_misses,
	// evt_l2_demand_misses,
	// evt_l2_rfo_misses,
	// evt_l2_data_misses,
	// evt_l2_code_misses,
	// evt_l2_pre_misses
	// evt_l2_all_pre,

	// evt_l2_in_all,
	// evt_l2_wb,
	// evt_l2_out_silent,
	// evt_l2_out_non_silent,
	// evt_l2_out_useless,
	// evt_l2_pre_misses,


	// evt_l2_all_demand,
	// evt_l2_demand_misses,
	// evt_stores_all
	
	// evt_l2_misses,
	// evt_l2_all_pre,

	// evt_l2_wb,
	// evt_l2_out_silent,
	// evt_l2_out_non_silent,
	// evt_l2_out_useless,
	// evt_l2_in_all,
	// evt_l2_pre_misses
};
#endif

	// evt_l2_misses,
	// evt_l2_all_pre,

	// evt_l2_wb,
	// evt_l2_out_silent,
	// evt_l2_out_non_silent,
	// evt_l2_out_useless,
	// evt_l2_in_all,
	// evt_l2_pre_misses

int recode_pmc_init(void)
{
	unsigned k;
	struct pmc_cfg cfgs[MAX_ID_PMC];

	on_each_cpu(pcpu_disable_pmc, NULL, 1);

	for (k = 0; k < MAX_ID_PMC; ++k) {
		cfgs[k].perf_evt_sel = pmc_events[k];
		/* PMCs setup */
		cfgs[k].usr = 1;
		cfgs[k].os = 1;
		cfgs[k].pmi = 0;
		cfgs[k].en = 1;

		pr_info("recode_pmc_init %llx\n", cfgs[k].perf_evt_sel);
	}

	on_each_cpu(pcpu_setup_pmc, cfgs, 1);

	// on_each_cpu(pcpu_enable_pmc, NULL, 1);

	return 0;
}

// TODO Implement
void recode_pmc_test(void)
{
	unsigned long flags = 0;
	u64 msr = 0, msr2 = 0, j = 0;
	u64 *r;
	struct pmc_cfg cfgs[2];
	unsigned i = 0;
	unsigned k = 0;
	struct pmc_logger *logger;
	unsigned cpu = get_cpu();
	u64 offcore1 = 0ULL;

	local_irq_save(flags);

	logger = this_cpu_read(pcpu_pmc_logger);
	
	pr_info("Running on CPU %u", cpu);

	// Program PMC
	// cfgs[k].perf_evt_sel = evt_umask_cmask(0xc2, 0x02, 0);
	cfgs[k].perf_evt_sel = evt_umask_cmask(0xb7, 0x01, 0);
	/* PMCs setup */
	cfgs[k].usr = 1;
	cfgs[k].os = 1;
	cfgs[k].pmi = 0;
	cfgs[k].en = 1;

	wrmsrl(MSR_CORE_PERFEVTSEL(k), cfgs[k].perf_evt_sel);
	pd_write_pmc(k, 0ULL);

	++k;

	cfgs[k].perf_evt_sel = evt_umask_cmask(0xb0, 0x04, 0);
	/* PMCs setup */
	cfgs[k].usr = 1;
	cfgs[k].os = 1;
	cfgs[k].pmi = 0;
	cfgs[k].en = 1;

	wrmsrl(MSR_CORE_PERF_FIXED_CTR_CTRL, 0x333);

	wrmsrl(MSR_CORE_PERFEVTSEL(k), cfgs[k].perf_evt_sel);
	pd_write_pmc(k, 0ULL);

	offcore1 |= BIT_ULL(1) | BIT_ULL(16); 


	wrmsrl(0x1a6, offcore1);
	r = vmalloc(PAGE_SIZE * 128);

	pcpu_enable_pmc(NULL);
	barrier();


	r[12134] = 33;

	r[12134 + 734] = 17;


	for (i = 0; i < PAGE_SIZE * 4; ++i) {
		// j += (u64) (logger + i) - (u64) (logger);
		j += r[i * 3];
	}

	barrier();
	pcpu_disable_pmc(NULL);

	pd_read_pmc(0, msr);
	pd_read_pmc(1, msr2);

	pr_info("Got value: %llu - %llu and result %llu (%u)", msr, msr2, j, i);

	local_irq_restore(flags);
	put_cpu();
}

void recode_pmc_fini(void)
{
	/* Nothing to do */
}

static bool sampling_enabled = false;

static void ctx_hook(struct task_struct *prev, bool prev_on, bool curr_on)
{
	int cpu = get_cpu();
	unsigned buff_idx;
	struct pmc_snapshot *ctx_snap;
	struct pmc_snapshot *delta_snap;
	struct pmc_logger *logger;

	u64 debug = 0;

	ctx_snap = this_cpu_ptr(&pcpu_pmc_snapshot_ctx);

	pcpu_disable_pmc(NULL);

	/* Crate a PMC snapshot */
	pd_read_fixed_pmc(0, ctx_snap->fixed0);
	pd_read_fixed_pmc(1, ctx_snap->fixed1);
	pd_read_fixed_pmc(2, ctx_snap->fixed2);
	pd_read_pmc(0, ctx_snap->pmc0);
	pd_read_pmc(1, ctx_snap->pmc1);
	pd_read_pmc(2, ctx_snap->pmc2);
	pd_read_pmc(3, ctx_snap->pmc3);
	pd_read_pmc(4, ctx_snap->pmc4);
	pd_read_pmc(5, ctx_snap->pmc5);
	pd_read_pmc(6, ctx_snap->pmc6);
	pd_read_pmc(7, ctx_snap->pmc7);

	pcpu_enable_pmc(NULL);

	debug = prev->pmc_user->fixed2;

	logger = this_cpu_read(pcpu_pmc_logger);
	buff_idx = logger->idx;
	// /* Log sample */
	if (buff_idx < BUFF_LENGTH && sampling_enabled) {
		delta_snap = logger->buff + buff_idx;

		/* We can spend ~10 more cycles */
		delta_snap->tsc = rdtsc_ordered();
		delta_snap->pid = prev->pid;
		
		if (prev->mm)
			delta_snap->pid |= BIT(31);

		if (prev_on)
			delta_snap->pid |= BIT(30);

		FULL_PMC_ACTION(COMPUTE_DELTA);

		FULL_PMC_ACTION(ADD_DELTA);
		
		logger->idx = buff_idx + 1;
		if (buff_idx == BUFF_LENGTH - 1) {
			pr_info("[Core %u] PMC_BUFFER SPACE USED UP\n", cpu);
		} else if (!(buff_idx % 1000)) {
			pr_info("[Core %u] PMC_BUFFER REACHED POS %u\n", cpu, buff_idx);
		}
	} else {
		/* Compute data since last CTXT */
		FULL_PMC_ACTION(COMPUTE_DATA);
	}

	/* Prev is updated */

	/* 
	 * NEW = in.fixed2
	 * OLD = out.fixed2
	 * UPD = ctx.fixed2
	 */
	// if (prev_on || curr_on) {
	// 	pr_info("[%u, %u(%u)->%u(%u)] 
	// 		NEWa: %llx, OLDa: %llx, SNAPn: %llx, SNAPo: %llx, UPD: %llx\n",
	// 		cpu, prev->pid, (!!prev->mm) ? 3 : 0, current->pid, (!!current->mm) ? 3 : 0,
	// 		prev->pmc_user->fixed2, prev->pmc_user->fixed2 - debug,
	// 		ctx_snap->fixed2, this_cpu_read(pcpu_pmc_snapshot_out.fixed2),
	// 		ctx_snap->fixed2 - this_cpu_read(pcpu_pmc_snapshot_out.fixed2));
	// }

	/* Update the snapshot */
	FULL_PMC_ACTION(TAKE_FX_SNAPSHOT);

	/* Prev is profiled */
	if (prev_on) {
		sampling_enabled = true;
	}


	if (curr_on) {
		sampling_enabled = true;
	}

	// end:
	put_cpu();
}

/* Register thread for profiling activity  */
int attach_process(pid_t pid)
{
	pr_info("Attaching pid %u\n", pid);

	this_cpu_write(pcpu_track.stop, 0);
	this_cpu_write(pcpu_track.kin0, 0);
	this_cpu_write(pcpu_track.kin1, 0);
	this_cpu_write(pcpu_track.kout0, 0);
	this_cpu_write(pcpu_track.kout1, 0);

	pid_register(pid);
	return 0;
}

/* Remove registered thread from profiling activity */
void detach_process(pid_t pid)
{
}

int register_ctx_hook(void)
{
	int err = 0;

	hook_register(&ctx_hook);
	// pid_register(16698);
	switch_hook_resume();
	// test_asm();
	return err;
}

void unregister_ctx_hook(void)
{
	hook_unregister();
}
