#include "tma_scheduler.h"
#include "device/proc.h"

/* Proc and Fops related to PMUDRV scheduler */

int scheduler_seq_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%u\n", scheduler_state);
	return 0;
}

int scheduler_open(struct inode *inode, struct file *file)
{
	return single_open(file, scheduler_seq_show, NULL);
}

static ssize_t scheduler_write(struct file *file, const char __user *buffer,
			   size_t count, loff_t *ppos)
{
	int err;
	uint scheduler;


	err = kstrtouint_from_user(buffer, count, 10, &scheduler);
	if (err)
		return err;

	if (err >= SCHEDULER_MAX)
		return -EINVAL;

	recode_set_scheduler(scheduler);

	return count;
}

#if KERNEL_VERSION(5, 6, 0) > LINUX_VERSION_CODE
static const struct file_operations scheduler_proc_fops = {
	.owner = THIS_MODULE,
	.open = scheduler_open,
	.read = seq_read,
	.write = scheduler_write,
	.release = single_release,
#else
static const struct proc_ops scheduler_proc_fops = {
	.proc_open = scheduler_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_write = scheduler_write,
	.proc_release = single_release,
#endif
};

int register_proc_scheduler(void)
{
	struct proc_dir_entry *dir;

	dir = proc_create(GET_PATH("scheduler"), 0666, NULL, &scheduler_proc_fops);

	return !dir;
}