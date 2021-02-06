#include <linux/module.h>
#include <linux/moduleparam.h>

#include "device/proc.h"
#include "recode.h"

static __init int recode_init(void)
{
	if(recode_data_init())
		goto no_data;

	init_proc();
        register_ctx_hook();

	if(recode_pmc_init())
		goto err;

	pr_info("module loaded\n");
	return 0;

err:
        unregister_ctx_hook();
        fini_proc();
	recode_data_fini();
no_data:
	return -1;
}

static void __exit recode_exit(void)
{
	recode_pmc_fini();
        unregister_ctx_hook();
        fini_proc();
	
	recode_data_fini();

	pr_info("hack_exit module removed\n");
}

module_init(recode_init);
module_exit(recode_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Stefano Carna");