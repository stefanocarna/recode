/* 
 * Proc and Fops related to PMC events
 */

#include "proc.h"
#include "../pmu/pmu.h"
#include "../recode_config.h"

#define DATA_HEADER "# PID,TRACKED,KTHREAD,CTX_EVT,TIME,TSC,INST,CYCLES,TSC_CYCLES,*"

static int sampling_hw_events_show(struct seq_file *m, void *v)
{
	unsigned k, i;

	seq_printf(m, "%s\n", DATA_HEADER);

	for (k = 0; k < gbl_nr_hw_events; ++k) {
		seq_printf(m, "%llx\t\t", gbl_hw_events[k]->mask);
			/* Check bit - Avoid comma*/
			for (i = 0; i < 64; i++) {
				/* Check bit */
				if (gbl_hw_events[k]->mask & BIT(i)) {
					seq_printf(m, ",%x", HW_EVENTS_BITS[i].raw);
				}
			}
			seq_printf(m, "\n");
	}

	return 0;
}

static int hw_events_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, sampling_hw_events_show, NULL);
}

#define MAX_USER_HW_EVENTS 32

static ssize_t hw_events_write(struct file *filp, const char __user *buffer_user,
			    size_t count, loff_t *ppos)
{
	int err, i = 0;
	char *p, *buffer;
	pmc_evt_code codes[MAX_USER_HW_EVENTS];

	buffer = (char *)kzalloc(sizeof(char) * count, GFP_KERNEL);
	err = copy_from_user((void *)buffer, (void *)buffer_user,
			     sizeof(char) * count);

	if (err)
		return err;

	/* Parse hw_events from user input */
	while ((p = strsep(&buffer, ",")) != NULL && i < MAX_USER_HW_EVENTS)
		sscanf(p, "%x", &codes[i++].raw);

	setup_hw_events_from_proc(codes, i);

	// recode_stop_and_reset();
	return count;
}

struct proc_ops hw_events_proc_fops = {
	.proc_open = hw_events_open,
	.proc_read = seq_read,
	.proc_write = hw_events_write,
	.proc_release = single_release,
};

int register_proc_hw_events(void)
{
	struct proc_dir_entry *dir;

	dir = proc_create(GET_PATH("hw_events"), 0666, NULL, &hw_events_proc_fops);

	return !dir;
}