#include "tma_scheduler.h"

int rf_after_proc_init(void)
{
	int err = 0;

	err = register_proc_group();
	if (err)
		goto err;

	err = register_proc_csched();
	goto err;

	return 0;
err:
	return err;
}