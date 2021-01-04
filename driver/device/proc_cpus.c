#include "proc.h"

/* 
 * Proc and Fops related to CPU device
 */

static void *cpu_logger_seq_start(struct seq_file *m, loff_t *pos)
{
	unsigned *i;
	struct pmc_logger *data;

	data = (struct pmc_logger *)PDE_DATA(file_inode(m->file));
	if (!data)
		goto err;

	if (*pos >= data->idx)
		goto err;

	i = vmalloc(sizeof(unsigned));
	*i = *pos;

	return i;
err:
	return NULL;
}

static void *cpu_logger_seq_next(struct seq_file *m, void *v, loff_t *pos)
{
	unsigned *i;
	struct pmc_logger *data;
	data = (struct pmc_logger *)PDE_DATA(file_inode(m->file));
	i = (unsigned *)v;

	if (*pos >= data->idx || *i >= data->idx)
		goto err;

	(*i)++;

	return i;
err:
	return NULL;
}

static int cpu_logger_seq_show(struct seq_file *m, void *v)
{
	unsigned k;
	unsigned *i;
	struct pmc_logger *data;
	struct pmcs_snapshot ps;
	
	data = (struct pmc_logger *)PDE_DATA(file_inode(m->file));
	i = (unsigned *)v;

	if (*i >= data->idx)
		goto err;

	ps = data->buff[*i];

	if (!(*i)) {
		seq_printf(m, "# PID,TSC,INST,CLOCKS,CLOCKS_TSC");
		for (k = 0; k < max_pmc_general; ++k) {
			seq_printf(m, ",%x", pmc_events[k]);
		}
		seq_printf(m, "\n");
	}

	// data->cpu, to get cpu number
	//	      pid | tsc	|	fixed	  |	general |usr|prof|

	seq_printf(m, "%u,%llu,%llu,%llu,%llu", 111, ps.tsc, ps.fixed[0],
		   ps.fixed[1], ps.fixed[2]);

	for (k = 0; k < max_pmc_general; ++k) {
		seq_printf(m, ",%llu", ps.general[k]);
	}
	// seq_printf(m, ",%u,%u\n", 1, 1);
	seq_printf(m, "\n");

	return 0;
err:
	return -1;
}

static void cpu_logger_seq_stop(struct seq_file *m, void *v)
{
	/* Nothing to free */
}

static struct seq_operations cpu_logger_seq_ops = {
	.start = cpu_logger_seq_start,
	.next = cpu_logger_seq_next,
	.stop = cpu_logger_seq_stop,
	.show = cpu_logger_seq_show
};

static int cpu_logger_open(struct inode *inode, struct file *filp)
{
	return seq_open(filp, &cpu_logger_seq_ops);
}

struct file_operations cpu_logger_proc_fops = {
	.open = cpu_logger_open,
	.llseek = seq_lseek,
	.read = seq_read,
	.release = seq_release,
};

int register_proc_cpus(void)
{
	unsigned cpu;
	char name[17];
	struct proc_dir_entry *dir;
	struct proc_dir_entry *tmp_dir;

	dir = proc_mkdir(GET_PATH("cpus"), NULL);

	for_each_online_cpu (cpu) {
		sprintf(name, "cpu%u", cpu);

		/* memory leak when releasing */
		tmp_dir =
			proc_create_data(name, 0444, dir, &cpu_logger_proc_fops,
					 per_cpu(pcpu_pmc_logger, cpu));

		// TODO Add cleanup code
		if (!tmp_dir)
			return -1;
	}

	return 0;
}
