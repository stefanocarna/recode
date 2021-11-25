#include <linux/module.h>
#include <linux/moduleparam.h>

#include "pmu_abi.h"

#include "device/proc.h"
#include "recode.h"
#include "plugins/recode_tma.h"

#include "recode_tma.h"

static __init int recode_init(void)
{
	pr_debug("Mounting with DEBUG enabled\n");

	pr_info("Mounting with TMA module\n");

	if (register_system_hooks())
		goto no_hooks;

	recode_init_proc();

	if (recode_pmc_init()) {
		pr_err("Cannot initialize PMCs\n");
		goto no_pmc;
	}

	recode_tma_init();

	/* Enable PMU module support */
	pmudrv_set_state(true);

	pr_info("Module loaded\n");
	return 0;

no_pmc:
	recode_fini_proc();
	unregister_system_hooks();
no_hooks:
	recode_data_fini();
	return -1;
}

static void __exit recode_exit(void)
{
	recode_tma_fini();

	recode_pmc_fini();
	recode_fini_proc();
	unregister_system_hooks();

	pr_info("Module removed\n");
}

module_init(recode_init);
module_exit(recode_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Stefano Carna'");
