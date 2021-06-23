#include <linux/sched.h>
#include <asm/perf_event.h>

#include "recode.h"
#include "pt_config.h"

u64 perf_global_ctrl = 0xFULL | BIT_ULL(32) | BIT_ULL(33) | BIT_ULL(34);
u64 fixed_ctrl = 0;
// u64 fixed_ctrl = 0x3B3; // Enable OS + USR
// u64 fixed_ctrl = 0x3A3; // Enable USR only
// u64 fixed_ctrl = 0x393; // Enable OS only

unsigned __read_mostly fixed_pmc_pmi = 1; // PMC with PMI active
unsigned __read_mostly max_pmc_fixed = 3;
unsigned __read_mostly max_pmc_general = 4;

DEFINE_PER_CPU(bool ,pcpu_pmcs_active) = false;

DEFINE_PER_CPU(unsigned long, pt_buffer_cpu) = 0;
DEFINE_PER_CPU(u64 *, topa_cpu);

static void dump_pt_state(void)
{
	u64 msr;
	unsigned cpu = get_cpu();

	rdmsrl(MSR_IA32_RTIT_CTL, msr);
	pr_warn("[%u] IA32_RTIT_CTL: %llx\n", cpu, msr);

	rdmsrl(MSR_IA32_RTIT_OUTPUT_MASK_PTRS, msr);
	pr_warn("[%u] IA32_RTIT_OUTPUT_MASK_PTRS.MaskOrTableOffset: %llx\n", 
		 cpu, ((msr) << 7) & (BIT(32) - 1));

	pr_warn("[%u] IA32_RTIT_OUTPUT_MASK_PTRS.OutputOffset: %llx\n", 
		 cpu, ((msr) >> 32));

	rdmsrl(MSR_IA32_RTIT_OUTPUT_BASE, msr);
	pr_warn("[%u] IA32_RTIT_OUTPUT_BASE: %llx\n", cpu, msr);

	put_cpu();
}

void get_machine_configuration(void)
{
	union cpuid10_edx edx;
	union cpuid10_eax eax;
	union cpuid10_ebx ebx;
	unsigned int unused;
	unsigned version;
	u64 msr;

	cpuid(10, &eax.full, &ebx.full, &unused, &edx.full);

	if (eax.split.mask_length < 7)
		return;

	version = eax.split.version_id;

	pr_info("PMU CONF:\n");
	// pr_info("Version: %u\n", version);
	pr_info("-- # Counters: %u\n", eax.split.num_counters);
	// pr_info("Counter's Bits: %u\n", eax.split.bit_width);
	// pr_info("Counter's Mask: %llx\n", (1ULL << eax.split.bit_width) - 1);

	// pr_info("Evt's Bits: %u\n", ebx.full);
	// pr_info("Evt's Mask: %x\n", eax.split.mask_length);

	pr_info("-- # PEBS EVTs: %x\n",
		min_t(unsigned, 8, eax.split.num_counters));

	max_pmc_general = eax.split.num_counters;
	perf_global_ctrl = (BIT(max_pmc_general) - 1) | BIT(32) | BIT(33) | BIT(34);

	rdmsrl(MSR_IA32_RTIT_CTL, msr);
	pr_info("Intel PT - IA32_RTIT_CTL: %llx\n", msr);
}

static void __setup_pmc_on_cpu(void *pmc_cfgs)
{
	u64 msr;
	unsigned k;
	struct pmc_evt_sel *cfgs = (struct pmc_evt_sel *) pmc_cfgs;

	if (!cfgs) {
		pr_warn("Cannot setup PMCs with a NULL conf\n");
		return;
	}

	/* Refresh APIC entry */
	apic_write(APIC_LVTPC, RECODE_PMI);

	/* Clear a possible stale state */
	rdmsrl(MSR_CORE_PERF_GLOBAL_STATUS, msr);
	wrmsrl(MSR_CORE_PERF_GLOBAL_OVF_CTRL, msr);

	/* Enable FREEZE_ON_PMI */
	wrmsrl(MSR_IA32_DEBUGCTLMSR, BIT(12));

	for (k = 0; k < max_pmc_general; ++k) {
		SETUP_GENERAL_PMC(k, cfgs[k].perf_evt_sel);
		WRITE_GENERAL_PMC(k, 0ULL);
	}

	for (k = 0; k < max_pmc_fixed; ++k) {
		if (k == fixed_pmc_pmi) {
			WRITE_FIXED_PMC(k, reset_period);
			fixed_ctrl |= (0xB << (k * 4)); /* 1011 -> PMI, USR, OS */
		} else {
			WRITE_FIXED_PMC(k, 0ULL);
			fixed_ctrl |= (0x3 << (k * 4)); /* 0011 -> USR, OS */
		}
	}

	/* Setup FIXED PMCs */
	wrmsrl(MSR_CORE_PERF_FIXED_CTR_CTRL, fixed_ctrl);
}

void setup_pmc_on_system(struct pmc_evt_sel *pmc_cfgs)
{
	on_each_cpu(__setup_pmc_on_cpu, pmc_cfgs, 1);
}

void read_all_pmcs(struct pmcs_snapshot *snapshot)
{
	unsigned pmc;
	if (!snapshot) {
		pr_warn("Cannot save PMCs on NULL snapshot\n");
		return;
	}

	snapshot->tsc = (u64)rdtsc_ordered();

	/* Read all active fixed pmcs */
	for_each_fixed_pmc(pmc)
	{
		if (perf_global_ctrl & BIT_ULL(pmc)) {
			snapshot->fixed[pmc] = READ_FIXED_PMC(pmc);
		}
	}
	/* Read all active general pmcs */
	for_each_general_pmc(pmc)
	{
		if (perf_global_ctrl & BIT_ULL(pmc)) {
			snapshot->general[pmc] = READ_GENERAL_PMC(pmc);
		}
	}
}

static void __enable_pmc_on_cpu(void *dummy)
{
	unsigned cpu = get_cpu();
	if (recode_state == OFF) {
		pr_warn("Cannot enable pmc on cpu %u while Recode is OFF\n",
			cpu);
		goto exit;
	}

	pr_warn("[%u] enabling PMU\n", cpu);
	per_cpu(pcpu_pmcs_active, cpu) = true;
	wrmsrl(MSR_CORE_PERF_GLOBAL_CTRL, perf_global_ctrl);
exit:
	put_cpu();
}

static void __disable_pmc_on_cpu(void *dummy)
{
	unsigned cpu = get_cpu();
	pr_warn("[%u] disabling PMU\n", cpu);
	per_cpu(pcpu_pmcs_active, cpu) = false;
	// per_cpu(pcpu_pmcs_active, get_cpu()) = false;
	wrmsrl(MSR_CORE_PERF_GLOBAL_CTRL, 0ULL);
	put_cpu();
}

void inline __attribute__((always_inline)) enable_pmc_on_cpu(void)
{
	__enable_pmc_on_cpu(NULL);
}

void inline __attribute__((always_inline)) disable_pmc_on_cpu(void)
{
	__disable_pmc_on_cpu(NULL);
}

void inline __attribute__((always_inline)) enable_pmc_on_system(void)
{
	on_each_cpu(__enable_pmc_on_cpu, NULL, 1);
}

void inline __attribute__((always_inline)) disable_pmc_on_system(void)
{
	on_each_cpu(__disable_pmc_on_cpu, NULL, 1);
}

unsigned pt_buffer_order = 1;

static int pt_buffer_init_on_cpu(void)
{
	int err = 0;
	unsigned cpu = get_cpu();
	
	u64 *topa;
	unsigned long pt_buffer;

	/* Allocate buffer */
	pt_buffer = __get_free_pages(GFP_KERNEL|__GFP_NOWARN|__GFP_ZERO, 
				     pt_buffer_order);
	if (!pt_buffer) {
		pr_err("[%u], Cannot allocate %ld KB buffer\n", cpu,
				(PAGE_SIZE << pt_buffer_order) / 1024);
		return -ENOMEM;
	}
	per_cpu(pt_buffer_cpu, cpu) = pt_buffer;
	
	topa = (u64 *)__get_free_page(GFP_KERNEL|__GFP_ZERO);
	if (!topa) {
		pr_err("[%u], Cannot allocate topa page\n", cpu);
		goto memory_err;
	}

	/* Assign buffer to current cpu */
	per_cpu(topa_cpu, cpu) = topa;

	/* Create circular topa table */
	topa[0] = (u64)__pa(pt_buffer) | (pt_buffer_order << TOPA_SIZE_SHIFT);
	// topa[1] = (u64)__pa(topa) | TOPA_END;

	pr_warn("Setup ToPA %llx\n", (u64)__pa(topa));
	pr_warn("ToPA[0] %llx\n", topa[0]);

	topa[1] = TOPA_STOP | TOPA_INT;

memory_err:
	put_cpu();
	return err;
}

static void pt_buffer_fini_on_cpu(void)
{
	unsigned cpu = get_cpu();
	
	u64 *topa;
	unsigned long pt_buffer;

	pt_buffer = per_cpu(pt_buffer_cpu, cpu);
	free_pages(per_cpu(pt_buffer_cpu, cpu), pt_buffer_order);

	topa = per_cpu(topa_cpu, cpu);
	free_page((unsigned long)topa);

	put_cpu();
}

void decode_pt_buffer(void)
{
	u64 msr;
	unsigned length, k = 0;
	unsigned char *buffer ;
	unsigned long pt_buffer;
	unsigned cpu = get_cpu();

	pt_buffer = per_cpu(pt_buffer_cpu, cpu);

	if (!pt_buffer)
		goto end;

	buffer = ((char *)pt_buffer);

	pr_warn("[%u} Decoding at %lx\n", cpu, (unsigned long) buffer);

	rdmsrl(MSR_IA32_RTIT_OUTPUT_MASK_PTRS, msr);

	pr_warn("[%u] IA32_RTIT_OUTPUT_MASK_PTRS.OutputOffset: %llx\n", 
		 cpu, ((msr) >> 32));

	length = (unsigned) ((msr) >> 32);

	while (length - k && k < 512) {
		pr_warn("[%u] byte %u: %x\n", cpu, k, buffer[k]);
		k++;
	}

end:
	put_cpu();
}

void enable_pt_on_cpu(void *dummy)
{
	u64 msr;
	unsigned cpu = get_cpu();

	pr_info("[%u] Enabling the PT support", cpu);

	rdmsrl(MSR_IA32_RTIT_CTL, msr);

	/* Check if PT is enabled and disable it*/
	if (msr & TRACE_EN)
		wrmsrl(MSR_IA32_RTIT_CTL, msr & ~TRACE_EN);

	/* Reset all relevant fields */
	msr &= ~(TSC_EN | CTL_OS | CTL_USER | CR3_FILTER | DIS_RETC | TO_PA |
		 CYC_EN | TRACE_EN | BRANCH_EN | CYC_EN | MTC_EN | MTC_EN |
		 MTC_MASK | CYC_MASK | PSB_MASK | ADDR0_MASK | ADDR1_MASK);

	msr |= TRACE_EN;
	msr |= TO_PA;

	/* ContextEn requires CPL or CR3 filtering */
	msr |= CTL_USER;


	/*
	 * PTWrite requires ContextEn & TriggerEn & FilterEn
	 * Unlike the latter, ContextEn is enabled by specific conditions. 
	 */

	// TODO + ToPA: multiple buffers
	pt_buffer_init_on_cpu();

	// TODO + SRO: single buffer

	/** In a primary implementation we use ToPA with a single buffer **/
	wrmsrl(MSR_IA32_RTIT_OUTPUT_BASE, __pa(this_cpu_read(topa_cpu)));

	wrmsrl(MSR_IA32_RTIT_OUTPUT_MASK_PTRS, 0ULL);

	pr_warn("Set ADDR to %lx\n", __pa(this_cpu_read(topa_cpu)));

	// + CR3 Filtering

	// + DisRETC: 0 to enable ret compression

	// + IP Ranges

	/* Configure the PT support */
	wrmsrl(MSR_IA32_RTIT_CTL, msr);

	dump_pt_state();

	put_cpu();
}

void disable_pt_on_cpu(void *dummy)
{
	u64 ctl, status;
	unsigned cpu = get_cpu();
	pr_info("[%u] Stopping the PT support", cpu);

	decode_pt_buffer();
	
	dump_pt_state();

	rdmsrl(MSR_IA32_RTIT_CTL, ctl);
	rdmsrl(MSR_IA32_RTIT_STATUS, status);

	pr_err("cpu %d, PT Status %llx\n", cpu, status);

	wrmsrl(MSR_IA32_RTIT_CTL, 0LL);
	wrmsrl(MSR_IA32_RTIT_STATUS, 0ULL);



	if (!(ctl & TRACE_EN))
		pr_warn("cpu %d, trace was not enabled on stop, ctl %llx, status %llx\n",
				cpu, ctl, status);
	if (status & PT_ERROR) {
		pr_err("cpu %d, error happened: status %llx\n", cpu, status);
		wrmsrl(MSR_IA32_RTIT_STATUS, status & ~(BIT(4) | BIT(5)));
		pr_warn("cpu %d, try to write %llx\n", cpu, status & ~(BIT(4) | BIT(5)));
		rdmsrl(MSR_IA32_RTIT_STATUS, status);
		pr_err("cpu %d, Second read status %llx\n", cpu, status);
	}

	wrmsrl(MSR_IA32_RTIT_CTL, 0LL);
	wrmsrl(MSR_IA32_RTIT_OUTPUT_BASE, 0ULL);
	wrmsrl(MSR_IA32_RTIT_OUTPUT_MASK_PTRS, 0ULL);
	pt_buffer_fini_on_cpu();

	dump_pt_state();

	put_cpu();
}