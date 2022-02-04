#include <linux/module.h>
#include <linux/moduleparam.h>

#include "pmu_abi.h"

#include "device/proc.h"
#include "recode.h"

#include "journal_base.h"

struct hw_events *gbl_hw_events;

static int register_hw_events(int hw_evts_cnt)
{
	int i;

	pmc_evt_code *pmc_evt_codes =
		kmalloc_array(hw_evts_cnt, sizeof(*pmc_evt_codes), GFP_KERNEL);

	if (!pmc_evt_codes)
		return -ENOMEM;

	for (i = 0; i < hw_evts_cnt; ++i)
		pmc_evt_codes[i].raw = HW_EVT_COD(iund_core);

	gbl_hw_events = create_hw_events(pmc_evt_codes, hw_evts_cnt);

	if (!gbl_hw_events)
		return -ENOMEM;

	setup_hw_events_global(gbl_hw_events);

	return 0;
}

static void unregister_hw_events(void)
{
	destroy_hw_events(gbl_hw_events);
}

atomic_t tracked_pmi;

// static void on_pmi_callback(uint cpu, struct pmus_metadata *pmus_metadata)
// {
// 	if (recode_state == OFF)
// 		return;

// 	if (query_tracked(current)) {
// 		atomic_inc(&tracked_pmi);
// 	}
// }

int rf_after_module_init(void)
{
	pr_info("Mounting with TMA module\n");

	if (register_hw_events(1)) {
		pr_err("Cannot register hw_events\n");
		return -1;
	}
	
	return 0;
}

// static __init int recode_init(void)
// {
// 	pr_debug("Mounting with DEBUG enabled\n");

// 	pr_info("Mounting with TMA module\n");

// 	if (register_system_hooks()) {
// 		pr_err("Cannot register tracepoints\n");
// 		goto no_hooks;
// 	}

// 	recode_init_proc();

// 	register_proc_info();

// 	if (recode_pmc_init()) {
// 		pr_err("Cannot initialize PMCs\n");
// 		goto no_pmc;
// 	}

// 	register_on_pmi_callback(on_pmi_callback);

// 	/* Enable PMU module support */
// 	pmudrv_set_state(true);

// 	if (register_hw_events(1)) {
// 		pr_err("Cannot register hw_events\n");
// 		goto no_hw_evts;
// 	}

// 	pr_info("Module loaded\n");
// 	return 0;

// no_hw_evts:
// 	pmudrv_set_state(false);
// 	recode_pmc_fini();
// no_pmc:
// 	recode_fini_proc();
// 	unregister_system_hooks();
// no_hooks:
// 	recode_data_fini();
// 	return -1;
// }

void rf_before_module_fini(void)
{
	unregister_hw_events();
}

// static void __exit recode_exit(void)
// {
// 	register_on_pmi_callback(NULL);

// 	unregister_hw_events();
// 	unregister_system_hooks();

// 	recode_pmc_fini();
// 	recode_fini_proc();

// 	pr_info("Module removed\n");
// }

// module_init(recode_init);
// module_exit(recode_exit);

// MODULE_LICENSE("GPL");
// MODULE_AUTHOR("Stefano Carna'");
