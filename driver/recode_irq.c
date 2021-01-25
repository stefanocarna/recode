#include "dependencies.h"
#include "recode.h"

// #include <asm/irq_regs.h>

int pmi_recode(void)
{
	u64 global;
	unsigned handled = 0, loops = 0;
	unsigned long flags = 0;
	// struct pt_regs *regs;

	atomic_inc(&active_pmis);

	irqs_disabled_flags(flags);

	/* Read the PMCs state */
	rdmsrl(MSR_CORE_PERF_GLOBAL_STATUS, global);

	/* TODO REMOVE */

	/* Nothing to do here */
	if (!global) {
		pr_info("[%u] Got PMI %llx - NO GLOBAL - FIXED1 %llx\n", 
		smp_processor_id(), global, READ_FIXED_PMC(1));
		goto no_pmi;
	}

	/* This IRQ is not originated from PMC overflow */
	if(!(global & PERF_GLOBAL_CTRL_FIXED1_MASK)) {
		pr_warn("Something triggered pmc_detection_interrupt line\n\
			MSR_CORE_PERF_GLOBAL_STATUS: %llx\n", global);
		goto no_pmi;
	}
	
	/* 
	 * The current implementation of this function does not
	 * provide a sliding window for a discrete samples collection.
	 * If a PMI arises, it means that there is a pmc multiplexing
	 * request. 	 
	 */

again:
	if (++loops > 100) {
		wrmsrl(MSR_CORE_PERF_GLOBAL_CTRL, 0ULL);
		pr_warn("[%u] LOOP STUCK - STOP PMCs\n", smp_processor_id());
		goto end;
	}

	// regs = __this_cpu_read(irq_regs);
	// pr_warn("[%u] CS: %lx\n", smp_processor_id(), regs->cs);

	pmc_evaluate_activity(current, is_pid_tracked(current->tgid), false);

	handled++;

	WRITE_FIXED_PMC(1, reset_period);
no_pmi:

	apic_write(APIC_LVTPC, RECODE_PMI);

	if (global) {
		wrmsrl(MSR_CORE_PERF_GLOBAL_OVF_CTRL, global);
	} else { 
		/* See arc/x86/intel/events/core.c:intel_pmu_handle_irq_v4 */
		rdmsrl(MSR_CORE_PERF_GLOBAL_STATUS, global);
		pr_warn("PMI end - GLOBAL: %llx\n", global);
		
		goto again;
	}

end:
	local_irq_restore(flags);
	atomic_dec(&active_pmis);

        return handled;
}// handle_ibs_event