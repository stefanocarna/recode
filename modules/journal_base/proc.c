#include "device/proc.h"
#include "journal_base.h"

static int info_seq_show(struct seq_file *m, void *v)
{
	seq_printf(m, "TRACKED PMIs %u\n", atomic_read(&tracked_pmi));

	return 0;
}

static int info_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, info_seq_show, NULL);
}

static ssize_t info_write(struct file *file, const char __user *buffer,
			   size_t count, loff_t *ppos)
{
	int err;
	uint cmd;

	err = kstrtouint_from_user(buffer, count, 10, &cmd);
	if (err)
		return -ENOMEM;

	if (cmd == 0)
		atomic_set(&tracked_pmi, 0);

	return count;
}

#if KERNEL_VERSION(5, 6, 0) > LINUX_VERSION_CODE
struct file_operations info_proc_fops = {
	.open = info_open,
	.read = seq_read,
	.write = info_write,
	.release = single_release,
#else
struct proc_ops info_proc_fops = {
	.proc_open = info_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_write = info_write,
	.proc_release = single_release,
#endif
};

int register_proc_info(void)
{
	struct proc_dir_entry *dir;

	dir = proc_create(GET_PATH("info"), 0666, NULL, &info_proc_fops);

	return !dir;
}