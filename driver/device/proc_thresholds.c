/* 
 * Proc and Fops related to PMC thresholds
 */

#include "proc.h"

static int thresholds_show(struct seq_file *m, void *v)
{
	unsigned i;

	for (i = 0; i < NR_THRESHOLDS; i++) {
		seq_printf(m, "%u\t", thresholds[i]);
	}
	seq_printf(m, "\n");

	return 0;
}

static int thresholds_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, thresholds_show, NULL);
}

// static ssize_t thresholds_write(struct file *filp, const char __user *buffer_user,
// 			    size_t count, loff_t *ppos)
// {
// 	char *p, *buffer;
// 	// u64 sval;
// 	int err, i = 0;
// 	buffer = (char *)kzalloc(sizeof(char) * count, GFP_KERNEL);
// 	err = copy_from_user((void *)buffer, (void *)buffer_user,
// 			     sizeof(char) * count);
// 	if (err)
// 		return err;
// 	while (((p = strsep(&buffer, ",")) != NULL) && i < max_pmc_general) {
// 		sscanf(p, "%x", &pmc_thresholds[i]);
// 		i++;
// 	}
// 	while (i < max_pmc_general) {
// 		pmc_thresholds[i] = 0ULL;
// 		i++;
// 	}
// 	// recode_stop_and_reset();
// 	return count;
// }

struct file_operations thresholds_proc_fops = {
	.open = thresholds_open,
	.read = seq_read,
	// .write = thresholds_write,
	.release = single_release,
};

int register_proc_thresholds(void)
{
	struct proc_dir_entry *dir;

	dir = proc_create(GET_PATH("thresholds"), 0444 /* 0666 */, NULL,
			  &thresholds_proc_fops);

	return !dir;
}