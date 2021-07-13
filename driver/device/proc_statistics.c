/* 
 * Proc and Fops related to PMC statistics
 */

#include "proc.h"

static int sampling_statistics_show(struct seq_file *m, void *v)
{
	// seq_printf(m, "%d\n", atomic_read(&generated_pmis));

	return 0;
}

static int statistics_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, sampling_statistics_show, NULL);
}

static ssize_t statistics_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *ppos)
{
	char pname[256];
	unsigned uncopied;
	size_t n = count > 255 ? 255 : count;

	uncopied = copy_from_user((void *)pname, (void *)buffer,
			sizeof(char) * (n));
	
	if (uncopied) {
		pr_info("Cannot copy al bytes - remain %u\n", uncopied);
	}

	pname[n] = '\0';

	pr_info("*** *** *** *** *** *** ***\n");
	pr_info("*** Running BENCH: %s\n", pname);

        /* Retrieve pid task_struct */
	return count;
}

struct proc_ops statistics_proc_fops = {
	.proc_open = statistics_open,
	.proc_read = seq_read,
	.proc_release = single_release,
	.proc_write = statistics_write,
};

int register_proc_statistics(void)
{
	struct proc_dir_entry *dir;

	dir = proc_create(GET_PATH("statistics"), 0666, NULL, &statistics_proc_fops);

	return !dir;
}