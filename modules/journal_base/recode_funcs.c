#include "recode.h"
#include "pmu_abi.h"

// enum recode_state __read_mostly recode_state = OFF;

// int recode_pmc_init(void)
// {
// 	return 0;
// }

// void recode_pmc_fini(void)
// {
// 	// pr_info("PMU uninstalled\n");
// }

// void recode_data_fini(void)
// {
// }

// void recode_set_state(uint state)
// {
// 	if (recode_state == state)
// 		return;

// 	switch (state) {
// 	case OFF:
// 		pr_info("Recode state: OFF\n");
// 		recode_state = OFF;
// 		disable_pmcs_global();
// 		return;
// 	case SYSTEM:
// 		enable_pmcs_global();
// 		pr_info("Recode ready for SYSTEM\n");
// 		break;
// 	default:
// 		pr_warn("Recode invalid state\n");
// 		return;
// 	}

// 	recode_state = state;
// }

// /* Register process to activity profiler  */
// int attach_process(struct task_struct *tsk, char *gname)
// {
// 	int err;

// 	if (!tsk) {
// 		err = -EINVAL;
// 		pr_info("Cannot attach NULL task\n");
// 		goto end;
// 	}

// 	err = track_thread(tsk);

// 	if (err)
// 		pr_info("Error (%d) while Attaching process: [%u:%u]\n", err,
// 			tsk->tgid, tsk->pid);
// 	else
// 		pr_info("Attaching process: [%u:%u]\n", tsk->tgid, tsk->pid);

// end:
// 	return err;
// }

// /* Remove registered thread from profiling activity */
// void detach_process(struct task_struct *tsk)
// {
// 	pr_info("Detaching process: [%u:%u]\n", tsk->tgid, tsk->pid);
// 	untrack_thread(tsk);
// }