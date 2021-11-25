#include "device/proc.h"

#include "tma_scheduler.h"

/* Proc and Fops related to PMUDRV state */

static void *groups_seq_start(struct seq_file *m, loff_t *pos)
{
	loff_t *spos;

	if (*pos >= nr_gsteps)
		return NULL;

	spos = kmalloc(sizeof(loff_t), GFP_KERNEL);

	if (!spos)
		return NULL;

	*spos = *pos;
	return spos;
}

static void *groups_seq_next(struct seq_file *m, void *v, loff_t *pos)
{
	loff_t *spos = v;
	*pos = ++*spos;

	if (*pos >= nr_gsteps)
		return NULL;

	return spos;
}

static int groups_seq_show(struct seq_file *m, void *v)
{
	uint k;
	loff_t *spos = v;

	seq_printf(m, "ID %u\n", gsteps[*spos].id);

	seq_puts(m, "SET");
	for (k = 0; k < gsteps[*spos].nr_groups; ++k)
		seq_printf(m, " %u", gsteps[*spos].groups_id[k]);
	seq_puts(m, "\n");

	seq_printf(m, "SAMPLES %u\n", atomic_read(&gsteps[*spos].profile.nr_samples));
	seq_puts(m, "METRICS\n");

#define X_TMA_LEVELS_FORMULAS(name, idx)                                       \
	seq_puts(m, #name);                                             \
	for (k = 0; k < TRACK_PRECISION; ++k) {                                \
		seq_printf(m,                                                    \
			" %u",                                                 \
			atomic_read(                                           \
				&gsteps[*spos].profile.histotrack[idx][k]));   \
	}                                                                      \
	seq_puts(m, "\n");

	TMA_L3_FORMULAS
#undef X_TMA_LEVELS_FORMULAS

	return 0;
}

static void groups_seq_stop(struct seq_file *m, void *v)
{
	kfree(v);
}

struct seq_operations groups_seq_ops = { .start = groups_seq_start,
						.next = groups_seq_next,
						.stop = groups_seq_stop,
						.show = groups_seq_show };

int groups_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &groups_seq_ops);
}

#if KERNEL_VERSION(5, 6, 0) > LINUX_VERSION_CODE
static const struct file_operations groups_proc_fops = {
	.owner = THIS_MODULE,
	.open = groups_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
#else
static const struct proc_ops groups_proc_fops = {
	.proc_open = groups_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
#endif
};

int register_proc_group(void)
{
	struct proc_dir_entry *dir;

	dir = proc_create(GET_PATH("groups"), 0444, NULL, &groups_proc_fops);

	return !dir;
}