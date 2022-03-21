#include "tma_scheduler.h"

int rf_after_proc_init(void)
{
	int err = 0;

	err = register_proc_group();
	if (err)
		goto err;

	err = register_proc_scheduler();
	if (err)
		goto err;

	err = register_proc_histograms();
	if (err)
		goto err;

	err = register_proc_csched();
	if (err)
		goto err;

	err = register_proc_apps();
	if (err)
		goto err;

	return 0;
err:
	return err;
}