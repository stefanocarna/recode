#include "proc.h"
#include "logic/tma.h"

/* Proc and Fops related to PMUDRV tma */

static int tma_seq_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%u\n", tma_enabled);

	return 0;
}

static int tma_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, tma_seq_show, NULL);
}

static ssize_t tma_write(struct file *file, const char __user *buffer,
			   size_t count, loff_t *ppos)
{
	int err;
	uint tma;

	err = kstrtouint_from_user(buffer, count, 10, &tma);
	if (err)
		return -ENOMEM;

	if (tma > 2)
		return -EINVAL;

	pmudrv_set_tma(tma);

	return count;
}

#if KERNEL_VERSION(5, 6, 0) > LINUX_VERSION_CODE
struct file_operations tma_proc_fops = {
	.open = tma_open,
	.read = seq_read,
	.write = tma_write,
	.release = single_release,
#else
struct proc_ops tma_proc_fops = {
	.proc_open = tma_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_write = tma_write,
	.proc_release = single_release,
#endif
};

int pmu_register_proc_tma(void)
{
	struct proc_dir_entry *dir;

	dir = proc_create(GET_PATH("tma"), 0666, NULL, &tma_proc_fops);

	return !dir;
}