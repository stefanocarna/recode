#include "proc.h"

struct proc_dir_entry *pmu_root_pd_dir;

int pmu_init_proc(void)
{
	pmu_root_pd_dir = proc_mkdir(PROC_TOP, NULL);

	// register_proc_cpus();
	pmu_register_proc_frequency();
	// register_proc_sample_info();
	// register_proc_processes();
	pmu_register_proc_tma();
	pmu_register_proc_state();
	pmu_register_proc_reset();
	pmu_register_proc_info();
	pmu_register_proc_vector();

	return 0;
}

void pmu_fini_proc(void)
{
	proc_remove(pmu_root_pd_dir);
}
