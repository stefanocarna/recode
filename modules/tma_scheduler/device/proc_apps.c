#include <linux/vmalloc.h>

#include "device/proc.h"
#include "tma_scheduler.h"

/* 
 * Proc and Fops related to Tracked apps
 */

static ssize_t apps_write(struct file *file, const char __user *buffer,
			  size_t count, loff_t *ppos)
{
	int ret;
	struct task_struct *ts;
	char *safe_buf;

	pid_t pidp;
	char namep[TASK_COMM_LEN];

	safe_buf = memdup_user_nul(buffer, count);
	if (IS_ERR(safe_buf))
		return PTR_ERR(safe_buf);

	ret = sscanf(safe_buf, "%u:%s", &pidp, namep);
	if (ret != 2) {
		ret = -EFAULT;
		goto err_scan;
	}

	/* Retrieve pid task_struct */
	ts = get_pid_task(find_get_pid(pidp), PIDTYPE_PID);
	if (!ts) {
		pr_info("Cannot find task_struct for pid %u\n", pidp);
		ret = -EINVAL;
		goto err_pid;
	}

	attach_app(ts, namep);

	put_task_struct(ts);

	ret = count;

err_pid:
err_scan:
	kfree(safe_buf);
	return ret;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 6, 0)
struct file_operations apps_proc_fops = {
	.write = apps_write,
#else
struct proc_ops apps_proc_fops = {
	.proc_write = apps_write,
#endif
};

int register_proc_apps(void)
{
	struct proc_dir_entry *dir;

	dir = proc_create(GET_PATH("apps"), 0222, NULL, &apps_proc_fops);

	return !dir;
}