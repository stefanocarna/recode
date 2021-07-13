/* 
 * Proc and Fops related to PMC events
 */

#include "proc.h"
#include "../recode_config.h"

static int sampling_events_show(struct seq_file *m, void *v)
{
	unsigned i;

	for (i = 0; i < max_pmc_general; i++) {
		seq_printf(m, "%llx\t", pmc_events[i] & 0xffff);
	}
	seq_printf(m, "\n");

	return 0;
}

static int events_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, sampling_events_show, NULL);
}

static ssize_t events_write(struct file *filp, const char __user *buffer_user,
			    size_t count, loff_t *ppos)
{
	char *p, *buffer;
	// u64 sval;
	int err, i = 0;
	buffer = (char *)kzalloc(sizeof(char) * count, GFP_KERNEL);
	err = copy_from_user((void *)buffer, (void *)buffer_user,
			     sizeof(char) * count);
	if (err)
		return err;
	while (((p = strsep(&buffer, ",")) != NULL) && i < max_pmc_general) {
		sscanf(p, "%llx", &pmc_events[i]);
		i++;
	}
	while (i < max_pmc_general) {
		pmc_events[i] = 0ULL;
		i++;
	}
	// recode_stop_and_reset();
	return count;
}

struct proc_ops events_proc_fops = {
	.proc_open = events_open,
	.proc_read = seq_read,
	.proc_write = events_write,
	.proc_release = single_release,
};

int register_proc_events(void)
{
	struct proc_dir_entry *dir;

	dir = proc_create(GET_PATH("events"), 0666, NULL, &events_proc_fops);

	return !dir;
}