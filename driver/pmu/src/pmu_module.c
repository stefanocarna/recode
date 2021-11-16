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

	/* Setup fast IRQ */
	if (pmi_vector == NMI)
		err = pmi_nmi_setup();
	else
		err = pmi_irq_setup();

	if (err) {
		pr_err("Cannot initialize PMI vector\n");
		goto err;
	}

	if (pmu_global_init())
		goto no_cfgs;

	disable_pmcs_global();

	if (system_hooks_init())
		goto no_hooks;

	pmu_init_proc();

	return err;

no_hooks:
	pmc_global_fini();

no_cfgs:
	if (pmi_vector == NMI)
		pmi_nmi_cleanup();
	else
		pmi_irq_cleanup();

err:
	return -1;
}

static void __exit pmu_exit(void)
{
	/* Disable Recode */
	pmu_enabled = false;

	disable_pmcs_global();

	pmu_fini_proc();

	/* Wait for all PMIs to be completed */
	while (atomic_read(&active_pmis))
		;

	system_hooks_fini();

	pmc_global_fini();

	if (pmi_vector == NMI)
		pmi_nmi_cleanup();
	else
		pmi_irq_cleanup();

	pr_info("PMI uninstalled\n");
}

module_init(pmu_init);
module_exit(pmu_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Stefano Carna'");
