#include <linux/percpu-defs.h>
#include <linux/smp.h>
#include <asm/apic.h>
#include <asm/nmi.h>

#include "recode.h"
#include "recode_pmi.h"
#include "recode_pmu.h"
#include "recode_config.h"

static DEFINE_PER_CPU(u32, pcpu_lvt_bkp);
static struct nmiaction handler_na;

atomic_t active_pmis = ATOMIC_INIT(0);

// DEFINE_PER_CPU(unsigned long, pcpu_pmi_counter) = 2;
// DEFINE_PER_CPU(u64, pcpu_reset_period);

/*
 * Performance Monitor Interrupt handler
 */
static int pmi_handler(unsigned int cmd, struct pt_regs *regs)
{
	u64 global;
	unsigned handled = 0;
	unsigned cpu = get_cpu();

	atomic_inc(&active_pmis);	

	/* Read the PMCs state */
	rdmsrl(MSR_CORE_PERF_GLOBAL_STATUS, global);

	/* Nothing to do here */
	if (!global) {
		pr_info("[%u] Got PMI on vector %u - FIXED: %llx\n", 
		cpu, fixed_pmc_pmi, (u64) READ_FIXED_PMC(fixed_pmc_pmi));
		goto end;
	}

	/* This IRQ is not originated from PMC overflow */
	if(!(global & (PERF_GLOBAL_CTRL_FIXED0_MASK << fixed_pmc_pmi)) &&
	   !(global && PERF_COND_CHGD_IGNORE_MASK)) {
		pr_info("Something triggered PMI - GLOBAL: %llx\n", global);
		goto no_pmi;
	}
	
	/* 
	 * The current implementation of this function does not
	 * provide a sliding window for a discrete samples collection.
	 * If a PMI arises, it means that there is a pmc multiplexing
	 * request. 	 
	 */

	pmi_function(cpu);

	handled++;

	/* Backup last used reset_period */
	// this_cpu_write(pcpu_reset_period, reset_period);
	// WRITE_FIXED_PMC(fixed_pmc_pmi, this_cpu_read(pcpu_reset_period));
	WRITE_FIXED_PMC(fixed_pmc_pmi, reset_period);

no_pmi:
	wrmsrl(MSR_CORE_PERF_GLOBAL_OVF_CTRL, global);

end:

	if (recode_pmi_vector == NMI) {
		apic_write(APIC_LVTPC, LVT_NMI);

	} else {
		apic_write(APIC_LVTPC, RECODE_PMI);
		apic_eoi();
	}

	atomic_dec(&active_pmis);
	put_cpu();

        return handled;
}

static void pmi_lvt_setup_on_cpu(void *dummy)
{
	/* Backup old LVT entry */
	*this_cpu_ptr(&pcpu_lvt_bkp) = apic_read(APIC_LVTPC);
	apic_write(APIC_LVTPC, LVT_NMI);
}

static void pmi_lvt_cleanup_on_cpu(void *dummy)
{
	/* Restore old LVT entry */
	apic_write(APIC_LVTPC, *this_cpu_ptr(&pcpu_lvt_bkp));
}

/* Setup the PMI's NMI handler */
int pmi_nmi_setup(void)
{

	int err = 0;
	
	handler_na.handler = pmi_handler;
	handler_na.name = NMI_NAME;
	handler_na.flags = NMI_FLAG_FIRST;

	pr_info("Setting NMI as PMI vector\n");

	err = __register_nmi_handler(NMI_LOCAL, &handler_na);
	if (err)
		goto out;

	on_each_cpu(pmi_lvt_setup_on_cpu, NULL, 1);
out:
	return err;
}

void pmi_nmi_cleanup(void)
{
	on_each_cpu(pmi_lvt_cleanup_on_cpu, NULL, 1);
	unregister_nmi_handler(NMI_LOCAL, NMI_NAME);
}

#ifdef FAST_IRQ_ENABLED
static int fast_irq_pmi_handler(void)
{
	return pmi_handler(0, NULL);
}
#endif

int pmi_irq_setup(void)
{
#ifndef FAST_IRQ_ENABLED
	pr_info("PMI on IRQ not available on this kernel. Proceed with NMI\n");
	return pmi_nmi_setup();
}
#else
	int irq = 0;

	pr_info("Mapping PMI on IRQ #%u\n", RECODE_PMI);

	/* Setup fast IRQ */
	irq = request_fast_irq(RECODE_PMI, fast_irq_pmi_handler);

	if (irq != RECODE_PMI)
		return -1;

	return 0;
}
#endif

void pmi_irq_cleanup(void)
{
#ifndef FAST_IRQ_ENABLED
	pmi_nmi_cleanup();
}
#else
	int unused = 0;
	unused = free_fast_irq(RECODE_PMI);
}
#endif