#include <asm/tsc.h>
#include <linux/vmalloc.h>

#include "device/proc.h"
#include "recode_config.h"
#include "recode_collector.h"
#include "recode_groups.h"

static struct proc_dir_entry *dir;

static int gname_seq_show(struct seq_file *m, void *v)
{
	struct group_entity *gentity = m->private;

	seq_printf(m, "%s\n", gentity->name);
	return 0;
}

static int gname_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, gname_seq_show, PDE_DATA(file_inode(filp)));
}

static int gstats_seq_show(struct seq_file *m, void *v)
{
	int nr_samples = 0;
	unsigned long flags;
	struct group_entity *gentity = m->private;

	struct proc_list *cur;

	spin_lock_irqsave(&gentity->lock, flags);
	list_for_each_entry(cur, &gentity->p_list, list) {
		nr_samples += cur->proc->stats.nr_samples;
	}
	spin_unlock_irqrestore(&gentity->lock, flags);

	seq_printf(m, "%d\n", nr_samples);
	return 0;
}

static int gstats_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, gstats_seq_show, PDE_DATA(file_inode(filp)));
}

static int gprocesses_seq_show(struct seq_file *m, void *v)
{
	struct group_entity *gentity = m->private;

	seq_printf(m, "%d\n", gentity->nr_processes);
	return 0;
}

static int gprocesses_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, gprocesses_seq_show,
			   PDE_DATA(file_inode(filp)));
}

static ssize_t gprocesses_write(struct file *filp, const char __user *buffer,
				size_t count, loff_t *ppos)
{
	int ret;
	pid_t pidp;
	struct group_entity *gentity = PDE_DATA(file_inode(filp));

	ret = kstrtouint_from_user(buffer, count, 10, &pidp);
	if (ret) {
		pr_info("Cannot retrieve the pid\n");
		goto err_pid;
	}

	/* Retrieve pid task_struct */
	ret = register_process_to_group(pidp, gentity, NULL);

	if (ret) {
		pr_info("Cannot register pid %u to group %s\n", pidp,
			gentity->name);
		goto err_group;
	}

	ret = count;

err_pid:
err_group:
	return ret;
}

static int gactive_seq_show(struct seq_file *m, void *v)
{
	struct group_entity *gentity = m->private;

	seq_printf(m, "%d\n", gentity->active);
	return 0;
}

static int gactive_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, gactive_seq_show, PDE_DATA(file_inode(filp)));
}

static ssize_t gactive_write(struct file *filp, const char __user *buffer,
			     size_t count, loff_t *ppos)
{
	int ret;
	uint active;
	struct group_entity *gentity = PDE_DATA(file_inode(filp));

	ret = kstrtouint_from_user(buffer, count, 10, &active);
	if (ret) {
		pr_info("Cannot retrieve the pid\n");
		goto err_pid;
	}

	set_group_active(gentity, !!active);

	ret = count;

err_pid:
	return ret;
}

ONLY_OPEN_PROC_FOPS(gname, gname_open);
ONLY_OPEN_PROC_FOPS(gstats, gstats_open);
SIMPLE_RD_WR_PROC_FOPS(gprocesses, gprocesses_open, gprocesses_write);
SIMPLE_RD_WR_PROC_FOPS(gactive, gactive_open, gactive_write);

static int build_group_proc_entry(struct group_entity *gentity)
{
	// char buf[18];
	static struct proc_dir_entry *group_dir;

	// snprintf(buf, sizeof(buf), "%u", gentity->id);

	group_dir = proc_mkdir(gentity->name, dir);

	proc_create_data("name", 0444, group_dir, &gname_proc_fops, gentity);
	proc_create_data("stats", 0444, group_dir, &gstats_proc_fops, gentity);
	proc_create_data("active", 0666, group_dir, &gactive_proc_fops,
			 gentity);
	proc_create_data("processes", 0666, group_dir, &gprocesses_proc_fops,
			 gentity);

	// TODO Check
	return 0;
}

static ssize_t gcreate_write(struct file *filp, const char __user *buffer,
			     size_t count, loff_t *ppos)
{
	int ret;
	char *safe_buf;
	struct group_entity *gentity;

	char namep[TASK_COMM_LEN];

	safe_buf = memdup_user_nul(buffer, count);
	if (IS_ERR(safe_buf))
		return PTR_ERR(safe_buf);

	// TODO Check for duplicated groups
	ret = sscanf(safe_buf, "%s", namep);
	if (ret != 1) {
		ret = -EFAULT;
		goto err_scan;
	}

	gentity = create_group(namep, nr_groups, NULL);

	/* Create group proc entry */
	ret = build_group_proc_entry(gentity);

	if (ret) {
		destroy_group(gentity->id);
		return -ENOMEM;
	}

	return count;

err_scan:
	kfree(safe_buf);
	return ret;
}

static ssize_t gdestroy_write(struct file *filp, const char __user *buffer_user,
			      size_t count, loff_t *ppos)
{
	int val;
	int err;
	char buffer[18] = { 0 };
	int cn = count > 18 ? 18 : count;
	struct group_entity *gentity;

	err = copy_from_user((void *)buffer, (void *)buffer_user,
			     sizeof(char) * cn);

	sscanf(buffer, "%u", &val);

	gentity = get_group_by_id(val);

	remove_proc_subtree(gentity->name, dir);

	destroy_group(val);

	return count;
}

ONLY_WRITE_PROC_FOPS(gcreate, gcreate_write);
ONLY_WRITE_PROC_FOPS(gdestroy, gdestroy_write);

int recode_register_proc_groups(void)
{
	dir = proc_mkdir(GET_PATH("groups"), NULL);

	proc_create("create", 0222, dir, &gcreate_proc_fops);
	proc_create("destroy", 0222, dir, &gdestroy_proc_fops);

	return 0;
}
