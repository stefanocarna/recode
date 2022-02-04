#include "proc.h"
#include "pmi.h"
#include "pmu_config.h"

/* Proc and Fops related to PMUDRV vector */

static int vector_seq_show(struct seq_file *m, void *v)
{
	if (pmi_vector == NMI)
		seq_puts(m, "NMI\n");
#ifdef FAST_IRQ_ENABLED
	else if (pmi_vector == IRQ)
		seq_puts(m, "IRQ\n");
#endif
	else
		seq_puts(m, "Undefined vector... Oops\n");

	return 0;
}

static int vector_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, vector_seq_show, NULL);
}

static ssize_t vector_write(struct file *file, const char __user *buffer,
			    size_t count, loff_t *ppos)
{
	int err;
	uint vector;

	err = kstrtouint_from_user(buffer, count, 10, &vector);
	if (err)
		return -ENOMEM;

	if (vector >= MAX_VECTOR)
		pr_err("Cannot set vector type: %u - MAX %u\n", vector,
		       MAX_VECTOR);

	pmudrv_update_vector(vector);

	return count;
}

#if KERNEL_VERSION(5, 6, 0) > LINUX_VERSION_CODE
struct file_operations vector_proc_fops = {
	.open = vector_open,
	.read = seq_read,
	.write = vector_write,
	.release = single_release,
#else
struct proc_ops vector_proc_fops = {
	.proc_open = vector_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_write = vector_write,
	.proc_release = single_release,
#endif
};

int pmu_register_proc_vector(void)
{
	struct proc_dir_entry *dir;

	dir = proc_create(GET_PATH("vector"), 0666, NULL, &vector_proc_fops);

	return !dir;
}