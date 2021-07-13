#include <asm/tsc.h>
#include <linux/vmalloc.h>

#include "proc.h"
#include "../recode_config.h"

/* 
 * Proc and Fops related to CPU device
 */

static void *cpu_logger_seq_start(struct seq_file *m, loff_t *pos)
{
	u64 *i;
	unsigned k;
	struct pmc_logger *data;

	data = (struct pmc_logger *)PDE_DATA(file_inode(m->file));
	if (!data || !data->rd.head)
		goto err;

	// if (*pos >= data->idx)
	// 	goto err;
	i = vmalloc(sizeof(unsigned));
	*i = (*pos);

	/* Print labels */
	if (!(*i)) {
		seq_printf(m, "# PID,TIME,TSC,INST,CYCLES,TSC_CYCLES");
		for (k = 0; k < max_pmc_general; ++k) { 
			if (pmc_events[k]) {
				seq_printf(m, ",%llx", pmc_events[k]);
			}
		}
		seq_printf(m, "\n");
	}

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

	if (!data->rd.head)
		goto err;

	// if (*pos >= data->idx || *i >= data->idx)
	// 	goto err;

	(*i)++;

	return i;
err:
	return NULL;
}

static int cpu_logger_seq_show(struct seq_file *m, void *v)
{
	u64 time;
	unsigned j;
	unsigned k;
	unsigned *i;
	struct pmc_logger *data;
	struct pmcs_snapshot ps;
	struct pmcs_snapshot_ring *ring;
	
	data = (struct pmc_logger *)PDE_DATA(file_inode(m->file));
	ring = data->rd.head;
	i = (unsigned *)v;

	if (!i || !ring)
		goto err;

	// /* Print labels */
	// if (!(*i)) {
	// 	seq_printf(m, "# PID,TIME,TSC,INST,CYCLES,TSC_CYCLES");
	// 	for (k = 0; k < max_pmc_general; ++k) { 
	// 		seq_printf(m, ",%llx", pmc_events[k]);
	// 	}
	// 	seq_printf(m, "\n");
	// }

	/* Print the whole ring */
	for (j = 0; j < ring->length; ++j) {
		ps = ring->buff[j];
		// | pid | time | tsc | fixed | general | usr | prof |
		/* Compute milliseconds */
		time = ps.tsc; // native_sched_clock_from_tsc(ps.tsc) / 1000000;
		seq_printf(m, "%u,%llu,%llu,%llu,%llu,%llu", 111, time, ps.tsc,
			ps.fixed[0], ps.fixed[1], ps.fixed[2]);

		for (k = 0; k < max_pmc_general; ++k) {
			if (pmc_events[k]) {
				seq_printf(m, ",%llu", ps.general[k]);
			}
		}
		// seq_printf(m, ",%u,%u\n", 1, 1);
		seq_printf(m, "\n");
	}

	push_ps_ring(&data->chain, pop_ps_ring(&data->rd));
		
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

int register_proc_rtcpus(void)
{
	unsigned cpu;
	char name[17];
	struct proc_dir_entry *dir;
	struct proc_dir_entry *tmp_dir;

	dir = proc_mkdir(GET_PATH("rtcpus"), NULL);

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
