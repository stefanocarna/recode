#include "proc.h"
#include "logic/tma.h"

/* Proc and Fops related to PMUDRV tma */

static int tma_mode_seq_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%u\n", tma_enabled);

	return 0;
}

static int tma_mode_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, tma_mode_seq_show, NULL);
}

static ssize_t tma_mode_write(struct file *file, const char __user *buffer,
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


static int tma_level_seq_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%u\n", tma_max_level);

	return 0;
}

static int tma_level_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, tma_level_seq_show, NULL);
}

static ssize_t tma_level_write(struct file *file, const char __user *buffer,
			 size_t count, loff_t *ppos)
{
	int err;
	uint tma;

	err = kstrtouint_from_user(buffer, count, 10, &tma);
	if (err)
		return -ENOMEM;

	if (tma > DEFAULT_TMA_MAX_LEVEL)
		return -EINVAL;

	pmudrv_set_tma_max_level(tma);

	return count;
}

#if KERNEL_VERSION(5, 6, 0) > LINUX_VERSION_CODE
struct file_operations tma_mode_proc_fops = {
	.open = tma_mode_open,
	.read = seq_read,
	.write = tma_write,
	.release = single_release,
#else
struct proc_ops tma_mode_proc_fops = {
	.proc_open = tma_mode_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_write = tma_mode_write,
	.proc_release = single_release,
#endif
};

#if KERNEL_VERSION(5, 6, 0) > LINUX_VERSION_CODE
struct file_operations tma_level_proc_fops = {
	.open = tma_level_open,
	.read = seq_read,
	.write = tma_level_write,
	.release = single_release,
#else
struct proc_ops tma_level_proc_fops = {
	.proc_open = tma_level_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_write = tma_level_write,
	.proc_release = single_release,
#endif
};

int pmu_register_proc_tma(void)
{
	struct proc_dir_entry *dir_mode;
	struct proc_dir_entry *dir_level;

	dir_mode = proc_create(GET_PATH("tma_mode"), 0666, NULL,
			       &tma_mode_proc_fops);
	dir_level = proc_create(GET_PATH("tma_level"), 0666, NULL,
				&tma_level_proc_fops);

	return !dir_mode && !dir_level;
}