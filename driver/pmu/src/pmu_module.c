// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note

#include <linux/module.h>
#include <linux/moduleparam.h>

#include "pmi.h"
#include "pmu.h"
#include "pmu_low.h"
#include "pmu_core.h"
#include "pmu_config.h"
#include "proc/proc.h"

static __init int pmu_init(void)
{
	int err = 0;

	/* READ MACHINE CONFIGURATION */
	get_machine_configuration();

	err = pmi_setup();
	if (err) {
		pr_err("Cannot initialize PMI vector\n");
		goto err;
	}

	err = pmu_global_init();
	if (err)
		goto no_cfgs;

	err = pmu_init_proc();
	if (err)
		goto no_proc;

	pr_info("Module installed\n");

	return 0;

no_proc:
	pmu_global_fini();
no_cfgs:
	pmi_cleanup();

err:
	return err;
}

static void __exit pmu_exit(void)
{
	/* Disable Recode */
	pmu_enabled = false;

	disable_pmcs_global();

	/* Wait for all PMIs to be completed */
	while (atomic_read(&active_pmis))
		;

	pmu_fini_proc();

	pmu_global_fini();

	pmi_cleanup();

	pr_info("Module uninstalled\n");
}

module_init(pmu_init);
module_exit(pmu_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Stefano Carna'");
