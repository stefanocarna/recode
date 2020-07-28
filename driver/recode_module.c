#include <linux/module.h>
#include <linux/moduleparam.h>

#include "recode_core.h"
#include "device/proc.h"

static __init int recode_init(void)
{
	if(recode_data_init())
		return -1;


	init_dynamic_proc();
        register_ctx_hook();
	recode_pmc_init();
	recode_pmc_test();

	pr_info("hack_init module install\n");
	return 0;
}

static void __exit recode_exit(void)
{
	recode_pmc_fini();
        unregister_ctx_hook();
        fini_dynamic_proc();
	
	recode_data_fini();

	pr_info("hack_exit module removed\n");
}

module_init(recode_init);
module_exit(recode_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Stefano Carna");