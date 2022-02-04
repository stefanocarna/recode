#include <linux/module.h>
#include <linux/moduleparam.h>

#include "pmu_abi.h"

#include "device/proc.h"
#include "recode.h"

#ifdef TMA_MODULE_ON
#include "logic/recode_tma.h"
#endif

__weak int rf_after_module_init(void)
{
	/* Nothing to do */
	return 0;
}

__weak void rf_before_module_fini(void)
{
	/* Nothing to do */
}

static __init int recode_init(void)
{
	pr_debug("Mounting with DEBUG enabled\n");

// #ifdef TMA_MODULE_ON
// 	pr_info("Mounting with TMA module\n");
// #endif
// #ifdef SECURITY_MODULE_ON
// 	pr_info("Mounting with SECURITY module\n");
// #endif

	if (recode_groups_init()) {
		pr_err("Cannot initialize groups\n");
		goto no_groups;
	}

	if (recode_data_init()) {
		pr_err("Cannot initialize data\n");
		goto no_data;
	}

	if (system_hooks_init())
		goto no_hooks;

	if(recode_init_proc())
		goto no_proc;

	register_on_pmi_callback(rf_on_pmi_callback);

	// if (recode_pmc_init()) {
	// 	pr_err("Cannot initialize PMCs\n");
	// 	goto no_pmc;
	// }

// #ifdef TMA_MODULE_ON
// 	recode_tma_init();
// #endif
// #ifdef SECURITY_MODULE_ON
// 	recode_security_init();
// #endif

	if (rf_after_module_init())
		goto err;

	/* Enable PMU module support */
	pmudrv_set_state(true);

	pr_info("Module loaded\n");
	return 0;
err:
// no_pmc:
	recode_fini_proc();
no_proc:
	system_hooks_fini();
no_hooks:
	recode_data_fini();
no_data:
	recode_groups_fini();
no_groups:
	return -1;
}

static void __exit recode_exit(void)
{

// #ifdef TMA_MODULE_ON
// 	recode_tma_fini();
// #endif
// #ifdef SECURITY_MODULE_ON
// 	recode_security_init();
// #endif

	pmudrv_set_state(false);

	rf_before_module_fini();

	// recode_pmc_fini();
	
	register_on_pmi_callback(NULL);
	
	recode_fini_proc();

	system_hooks_fini();

	recode_data_fini();

	recode_groups_fini();

	pr_info("Module removed\n");
}

module_init(recode_init);
module_exit(recode_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Stefano Carna'");
