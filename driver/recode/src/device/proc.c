#include "device/proc.h"

#define SUCCESS_OR_EXIT(f)                                                     \
	do {                                                                   \
		err = f();                                                     \
		if (err)                                                       \
			goto no_proc;                                          \
	} while (0)

struct proc_dir_entry *root_pd_dir;

__weak int rf_after_proc_init(void)
{
	/* Do nothing */
	return 0;
}

__weak void rf_before_proc_fini(void)
{
	/* Do nothing */
}

int recode_init_proc(void)
{
	int err = 0;

	root_pd_dir = proc_mkdir(PROC_TOP, NULL);

	if (!root_pd_dir)
		goto no_dir;

	SUCCESS_OR_EXIT(recode_register_proc_cpus);
	SUCCESS_OR_EXIT(recode_register_proc_processes);
	SUCCESS_OR_EXIT(recode_register_proc_state);
	SUCCESS_OR_EXIT(recode_register_proc_groups);

	
	if (rf_after_proc_init())
		goto no_proc;

	// #ifdef SECURITY_MODULE_ON
	// 	SUCCESS_OR_EXIT(recode_register_proc_mitigations);
	// 	SUCCESS_OR_EXIT(recode_register_proc_thresholds);
	// 	SUCCESS_OR_EXIT(recode_register_proc_security);
	// 	SUCCESS_OR_EXIT(recode_register_proc_statistics);
	// #endif
	return err;

no_proc:
	proc_remove(root_pd_dir);
no_dir:
	return err;
}

void recode_fini_proc(void)
{
	rf_before_proc_fini();

	proc_remove(root_pd_dir);
}
