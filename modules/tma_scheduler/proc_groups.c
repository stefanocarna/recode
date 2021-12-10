#include "device/proc.h"

#include "tma_scheduler.h"

/* Proc and Fops related to PMUDRV state */

static void *groups_seq_start(struct seq_file *m, loff_t *pos)
{
	loff_t *spos;

	if (*pos >= nr_g_evaluations)
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

	if (*pos >= nr_g_evaluations)
		return NULL;

	return spos;
}

static int groups_seq_show(struct seq_file *m, void *v)
{
	uint k;
	loff_t *spos = v;

	seq_printf(m, "ID %u\n", g_evaluations[*spos].id);
	seq_printf(m, "NAME %s\n", g_evaluations[*spos].gname);

	seq_puts(m, "SET");
	for (k = 0; k < g_evaluations[*spos].nr_groups; ++k)
		seq_printf(m, " %u", g_evaluations[*spos].groups_id[k]);
	seq_puts(m, "\n");
	seq_printf(m, "TASKS %d\n", g_evaluations[*spos].nr_active_tasks);
	seq_printf(m, "SAMPLES %u\n",
		   atomic_read(&g_evaluations[*spos].profile.nr_samples));
	seq_printf(m, "CPU_TIME %llu\n", g_evaluations[*spos].cpu_time);
	seq_printf(m, "TOT_TIME %llu\n", g_evaluations[*spos].total_time);
	seq_printf(m, "PROC_TIME %llu\n", g_evaluations[*spos].profile.time);

	seq_printf(m, "POWER_TIME %llu\n", g_evaluations[*spos].profile.time);
	seq_printf(m, "POWER_UNITS %llu %llu %llu\n",
		   g_evaluations[*spos].rapl.power_units[0],
		   g_evaluations[*spos].rapl.energy_units[0],
		   g_evaluations[*spos].rapl.time_units[0]);
	seq_printf(m, "POWERS %llu %llu %llu %llu %llu\n",
		   g_evaluations[*spos].rapl.energy_package[0],
		   g_evaluations[*spos].rapl.energy_pp0[0],
		   g_evaluations[*spos].rapl.energy_pp1[0],
		   g_evaluations[*spos].rapl.energy_rest[0],
		   g_evaluations[*spos].rapl.energy_dram[0]);

	seq_puts(m, "METRICS\n");

#define X_TMA_LEVELS_FORMULAS(name, idx)                                       \
	seq_puts(m, #name);                                                    \
	for (k = 0; k < TRACK_PRECISION; ++k) {                                \
		seq_printf(                                                    \
			m, " %u",                                              \
			atomic_read(                                           \
				&g_evaluations[*spos].profile.histotrack[idx][k]));   \
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