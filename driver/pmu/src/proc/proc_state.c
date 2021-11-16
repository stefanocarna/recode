#include "proc.h"

/* Proc and Fops related to PMUDRV state */

static int state_seq_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%u\n", pmu_enabled);

	/* TODO REMOVE */
	debug_pmu_state();

	return 0;
}

static int state_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, state_seq_show, NULL);
}

static ssize_t state_write(struct file *file, const char __user *buffer,
			   size_t count, loff_t *ppos)
{
	int err;
	uint state;

	err = kstrtouint_from_user(buffer, count, 10, &state);
	if (err)
		return -ENOMEM;

	pmudrv_set_state(!!state);

	return count;
}

#if KERNEL_VERSION(5, 6, 0) > LINUX_VERSION_CODE
struct file_operations state_proc_fops = {
	.open = state_open,
	.read = seq_read,
	.write = state_write,
	.release = single_release,
#else
struct proc_ops state_proc_fops = {
	.proc_open = state_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_write = state_write,
	.proc_release = single_release,
#endif
};

int pmu_register_proc_state(void)
{
	struct proc_dir_entry *dir;

	dir = proc_create(GET_PATH("state"), 0666, NULL, &state_proc_fops);

	return !dir;
}