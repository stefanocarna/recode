#include "device/proc.h"

#include "tma_scheduler.h"

/* Proc and Fops related to PMUDRV state */

static void *csched_seq_start(struct seq_file *m, loff_t *pos)
{
	loff_t *spos;

	if (*pos >= nr_cs_evaluations)
		return NULL;

	spos = kmalloc(sizeof(loff_t), GFP_KERNEL);

	if (!spos)
		return NULL;

	*spos = *pos;
	return spos;
}

static void *csched_seq_next(struct seq_file *m, void *v, loff_t *pos)
{
	loff_t *spos = v;
	*pos = ++*spos;

	if (*pos >= nr_cs_evaluations)
		return NULL;

	return spos;
}

static int csched_seq_show(struct seq_file *m, void *v)
{
	int i, j;
	loff_t *spos = v;

	seq_printf(m, "PARTS %u\n", cs_evaluations[*spos].nr_parts);

	for (i = 0; i < cs_evaluations[*spos].nr_parts; ++i) {

		for (j = 0; j < cs_evaluations[*spos].parts[i].nr_groups; ++j)
			seq_printf(m, " %u", cs_evaluations[*spos].parts[i].group_ids[j]);

		seq_puts(m, "\n");
	}

	seq_printf(m, "SCORE %llu\n", cs_evaluations[*spos].score);
	seq_printf(m, "RETIRE %llu\n", cs_evaluations[*spos].retire);
	seq_printf(m, "ENERGY %llu\n", cs_evaluations[*spos].energy);
	seq_printf(m, "OCCUPANCY %llu\n", cs_evaluations[*spos].occupancy);

	return 0;
}

static void csched_seq_stop(struct seq_file *m, void *v)
{
	kfree(v);
}

struct seq_operations csched_seq_ops = { .start = csched_seq_start,
					 .next = csched_seq_next,
					 .stop = csched_seq_stop,
					 .show = csched_seq_show };

int csched_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &csched_seq_ops);
}

#if KERNEL_VERSION(5, 6, 0) > LINUX_VERSION_CODE
static const struct file_operations csched_proc_fops = {
	.owner = THIS_MODULE,
	.open = csched_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
#else
static const struct proc_ops csched_proc_fops = {
	.proc_open = csched_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
#endif
};

int register_proc_csched(void)
{
	struct proc_dir_entry *dir;

	dir = proc_create(GET_PATH("csched"), 0444, NULL, &csched_proc_fops);

	return !dir;
}