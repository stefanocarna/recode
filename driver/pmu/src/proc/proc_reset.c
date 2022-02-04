// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note

#include "pmu_low.h"
#include "proc.h"

/* Proc and Fops related to PMC period */

static int reset_seq_show(struct seq_file *m, void *v)
{
	uint pmc;

	seq_printf(m, "PMIs %u\n", this_cpu_read(pcpu_pmus_metadata.pmi_counter));
	
	for_each_fixed_pmc(pmc)
		seq_printf(m, "FX_ctrl %u: %llx\n", pmc, READ_FIXED_PMC(pmc));

	for_each_general_pmc(pmc)
		seq_printf(m, "GP_ctrl %u: %llx\n", pmc, READ_GENERAL_PMC(pmc));

	reset_pmc_global();

	return 0;
}

int pmu_register_proc_reset(void)
{
	struct proc_dir_entry *dir;

	// dir = proc_create(GET_PATH("frequency"), 0666, NULL,
	// 		  &frequency_proc_fops);

	dir = proc_create_single_data(GET_PATH("reset"), 0444, NULL,
				      reset_seq_show, 0);

	return !dir;
}
