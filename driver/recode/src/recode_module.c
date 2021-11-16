#include <linux/module.h>
#include <linux/moduleparam.h>

#include "pmu_abi.h"

#include "device/proc.h"
#include "recode.h"

#ifdef TMA_MODULE_ON
#include "logic/recode_tma.h"
#endif

static __init int recode_init(void)
{
	pr_debug("Mounting with DEBUG enabled\n");

#ifdef TMA_MODULE_ON
	pr_info("Mounting with TMA module\n");
#endif
#ifdef SECURITY_MODULE_ON
	pr_info("Mounting with SECURITY module\n");
#endif

	if (recode_groups_init()) {
		pr_err("Cannot initialize groups\n");
		goto no_groups;
	}

	if (recode_data_init()) {
		pr_err("Cannot initialize data\n");
		goto no_data;
	}

	if (register_system_hooks())
		goto no_hooks;

	init_proc();

	if (recode_pmc_init()) {
		pr_err("Cannot initialize PMCs\n");
		goto no_pmc;
	}

#ifdef TMA_MODULE_ON
	recode_tma_init();
#endif
#ifdef SECURITY_MODULE_ON
	recode_security_init();
#endif

	/* Enable PMU module support */
	pmudrv_set_state(true);

	pr_info("Module loaded\n");
	return 0;

no_pmc:
	fini_proc();
	unregister_system_hooks();
no_hooks:
	recode_data_fini();
no_data:
no_groups:
	return -1;
}

static void __exit recode_exit(void)
{
#ifdef TMA_MODULE_ON
	recode_tma_fini();
#endif
#ifdef SECURITY_MODULE_ON
	recode_security_init();
#endif

	recode_pmc_fini();
	fini_proc();
	unregister_system_hooks();

	recode_data_fini();

	recode_groups_fini();

	pr_info("Module removed\n");
}

module_init(recode_init);
module_exit(recode_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Stefano Carna'");
