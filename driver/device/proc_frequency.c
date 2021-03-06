#include "proc.h"

/* 
 * Proc and Fops related to PMC period
 */

static int frequency_seq_show(struct seq_file *m, void *v)
{
	u64 val = reset_period;

	seq_printf(m, "%llx\n", val);

	return 0;
}

static int frequency_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, frequency_seq_show, NULL);
}

static ssize_t frequency_write(struct file *filp,
			       const char __user *buffer_user, size_t count,
			       loff_t *ppos)
{
	u64 val;
	int err;
	char *buffer;

	buffer = (char *)kzalloc(sizeof(char) * count, GFP_KERNEL);
	err = copy_from_user((void *)buffer, (void *)buffer_user,
			     sizeof(char) * count);

	sscanf(buffer, "%llx", &val);

	reset_period = val;

	return count;
}

struct file_operations frequency_proc_fops = {
	.open = frequency_open,
	.read = seq_read,
	.write = frequency_write,
	.release = single_release,
};

int register_proc_frequency(void)
{
	struct proc_dir_entry *dir;

	dir = proc_create(GET_PATH("frequency"), 0666, NULL,
			  &frequency_proc_fops);

	return !dir;
}