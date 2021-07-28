#include <asm/tsc.h>
#include <linux/vmalloc.h>

#include "proc.h"
#include "recode_config.h"
#include "recode_collector.h"

#define DATA_HEADER "# PID,TRACKED,KTHREAD,CTX_EVT,TIME,TSC,INST,CYCLES,TSC_CYCLES"

extern unsigned int tsc_khz;

/* 
 * Proc and Fops related to CPU device
 */

static void *cpu_logger_seq_start(struct seq_file *m, loff_t *pos)
{
	unsigned pmc;
	struct data_logger *data;

	data = (struct data_logger *)PDE_DATA(file_inode(m->file));
	if (!data)
		goto no_data;
	
	if (!check_log_sample(data))
		goto no_data;

	/* Print header */
	if (!(*pos)) {
		seq_printf(m, DATA_HEADER);
		for_each_general_pmc(pmc)
			if (pmc_events[pmc]) {
				seq_printf(m, ",%llx", pmc_events[pmc]);
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
	struct data_logger *data;
	data = (struct data_logger *)PDE_DATA(file_inode(m->file));
	
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
	unsigned pmc;
	struct data_logger *data;
	struct pmcs_snapshot *pmcs;
	struct data_logger_sample *sample;
	
	data = (struct data_logger *)PDE_DATA(file_inode(m->file));

	if (!v)
		goto err;
		
	sample = read_log_sample(data);

	if (!sample)
		goto err;

	pmcs = &sample->pmcs;
	time = pmcs->tsc / tsc_khz;

	seq_printf(m, "%u,%u,%u,%u,%llu,%llu,%llu,%llu,%llu", sample->id,
	           sample->tracked, sample->k_thread, sample->ctx_evt, time,
		   pmcs->tsc, pmcs->fixed[0], pmcs->fixed[1], pmcs->fixed[2]);

	for_each_general_pmc(pmc) {
		if (pmc_events[pmc]) {
			seq_printf(m, ",%llu", pmcs->general[pmc]);
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
					 per_cpu(pcpu_data_logger, cpu));

		// TODO Add cleanup code
		if (!tmp_dir)
			return -1;
	}

	return 0;
}
