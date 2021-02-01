/* 
 * Proc and Fops related to PMC statistics
 */

#include "proc.h"

static int sampling_statistics_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", atomic_read(&generated_pmis));

	return 0;
}

static int statistics_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, sampling_statistics_show, NULL);
}

struct file_operations statistics_proc_fops = {
	.open = statistics_open,
	.read = seq_read,
	.release = single_release,
};

int register_proc_statistics(void)
{
	struct proc_dir_entry *dir;

	dir = proc_create(GET_PATH("statistics"), 0444, NULL, &statistics_proc_fops);

	return !dir;
}