#include <asm/tsc.h>
#include <linux/vmalloc.h>

#include "proc.h"
#include "../recode_config.h"

extern unsigned int tsc_khz;

/* 
 * Proc and Fops related to CPU device
 */

static void *cpu_logger_seq_start(struct seq_file *m, loff_t *pos)
{
	unsigned k;
	struct pmc_logger *data;

	data = (struct pmc_logger *)PDE_DATA(file_inode(m->file));
	if (!data)
		goto no_data;
	
	if (!check_log_sample(data))
		goto no_data;

	/* Print header */
	if (!(*pos)) {
		seq_printf(m, "# PID,TIME,TSC,INST,CYCLES,TSC_CYCLES");
		for (k = 0; k < max_pmc_general; ++k)
			if (pmc_events[k]) {
				seq_printf(m, ",%llx", pmc_events[k]);
			}
		seq_printf(m, "\n");
	}

	return pos;

no_data:
	pr_warn("Cannot read data: %u\n", !data->rd.head);
	return NULL;
}

static void *cpu_logger_seq_next(struct seq_file *m, void *v, loff_t *pos)
{
	struct pmc_logger *data;
	data = (struct pmc_logger *)PDE_DATA(file_inode(m->file));
	
	(*pos)++;

	if (!check_log_sample(data))
		goto err;

	return pos;
err:
	return NULL;
}

static int cpu_logger_seq_show(struct seq_file *m, void *v)
{
	u64 time;
	unsigned k;
	struct pmc_logger *data;
	struct pmcs_snapshot *sample;
	
	data = (struct pmc_logger *)PDE_DATA(file_inode(m->file));

	if (!v)
		goto err;
		
	sample = read_log_sample(data);

	if (!sample)
		goto err;
	
	time = sample->tsc / tsc_khz;
	seq_printf(m, "%u,%llu,%llu,%llu,%llu,%llu", 111, time, sample->tsc,
		sample->fixed[0], sample->fixed[1], sample->fixed[2]);

	for (k = 0; k < max_pmc_general; ++k) {
		if (pmc_events[k]) {
			seq_printf(m, ",%llu", sample->general[k]);
		}
	}
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

struct proc_ops cpu_logger_proc_fops = {
	.proc_open = cpu_logger_open,
	.proc_lseek = seq_lseek,
	.proc_read = seq_read,
	.proc_release = seq_release,
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
