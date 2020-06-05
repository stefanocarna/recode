#include <linux/slab.h>
#include <linux/hashtable.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sched/task.h>
#include <linux/pid.h>

#include "dependencies.h"
#include "../recode_core.h"

DEFINE_HASHTABLE(pids_htable, 8);
static spinlock_t hash_lock;

#define LCK_HASH spin_lock(&hash_lock)
#define UCK_HASH spin_unlock(&hash_lock)

static struct proc_dir_entry *root_pd_dir;

// struct pid_proc_info {
// 	pid_t *pid;
// 	pid_t *tgid;
// 	struct proc_dir_entry * root;
// 	struct detection_data *dd;
// 	struct hlist_node node;		/* hashtable struct */
// };

// static int pid_state_seq_show(struct seq_file *m, void *v)
// {
// 	struct task_struct *ts;
// 	pid_t *pidp = (pid_t*)PDE_DATA(file_inode(m->file));
// 	if (!pidp) goto err;

// 	ts = get_pid_task(find_get_pid(*pidp), PIDTYPE_PID);
// 	if (!ts) goto err;

// 	seq_printf(m, "%u\n", !!(ts->detection_state & PMC_D_PROFILING));

// 	put_task_struct(ts);
// 	return 0;
// err:
// 	return -1;
// }

// static int pid_state_open(struct inode *inode, struct  file *filp)
// {
// 	return single_open(filp, pid_state_seq_show, NULL);
// }

// static ssize_t pid_state_write(struct file *filp,
// 		const char __user *buffer, size_t count, loff_t *ppos)
// {
// 	int err;
// 	pid_t *pidp;
// 	unsigned val;
// 	struct task_struct *ts;

// 	pidp = (pid_t*)PDE_DATA(file_inode(filp));

// 	if (!pidp) goto err;

// 	err = kstrtouint_from_user(buffer, count, 0, &val);

// 	ts = get_pid_task(find_get_pid(*pidp), PIDTYPE_PID);
// 	if (!ts) goto err_quiet;

// 	/* TODO - FIX it */
// 	if (val && !(ts->detection_state & PMC_D_PROFILING))
// 		ts->detection_state |= PMC_D_REQ_PROFILING;
// 	else
// 		ts->detection_state &= ~PMC_D_PROFILING;

// 	put_task_struct(ts);
// 	return count;
// err:
// 	pr_info("[state_write] Internal Error: cannot read file data\n");
// err_quiet:
// 	return -1;
// }

// struct file_operations pid_state_proc_fops = {
// 	.open = pid_state_open,
// 	.read = seq_read,
// 	.write = pid_state_write,
// 	.release = single_release,
// };

// static void *pid_samples_seq_start(struct seq_file *m, loff_t *pos)
// {
// 	unsigned long k;
// 	struct detection_data *data;
// 	struct pmc_sample_block *psb;

// 	data = (struct detection_data *)PDE_DATA(file_inode(m->file));
// 	if (!data || !data->psb_stored) goto err;

// 	psb = data->psb_stored;
// 	k = *pos;

// 	while (k--) {
// 		if (!psb->next) goto err;
// 		psb = psb->next;
// 	}

// 	return psb;
// err:
// 	return NULL;
// }

// static void *pid_samples_seq_next(struct seq_file *m, void *v, loff_t *pos)
// {
// 	// unsigned long k = 0;
// 	struct pmc_sample_block *psb;

// 	psb = (struct pmc_sample_block *)v;

// 	if (!psb || !psb->next) goto err;
// 	psb = psb->next;

// 	return psb;
// err:
// 	return NULL;
// }

// static int pid_samples_seq_show(struct seq_file *m, void *v)
// {
// 	unsigned i;
// 	struct pmc_sample *psp;
// 	struct pmc_sample_block *psb;

// 	psb = (struct pmc_sample_block *)v;
// 	if (!psb) goto err;

// 	for (i = 0, psp = psb->ps; i < psb->cnt; ++i, ++psp) {

// 		seq_printf(m, "%llu\t%llu\t%llu\t%llu\t%llu\t%llu\n",
// 		    psp->fixed0, psp->fixed1, psp->pmc0, psp->pmc1,
// 		    psp->pmc2, psp->pmc3);
// 	}

// 	return 0;
// err:
// 	return -1;
// }

// static void pid_samples_seq_stop(struct seq_file *m, void *v)
// {
// 	/* Nothing to free */
// }

// static struct seq_operations pid_samples_seq_ops = {
// 	.start = pid_samples_seq_start,
// 	.next = pid_samples_seq_next,
// 	.stop = pid_samples_seq_stop,
// 	.show = pid_samples_seq_show
// };

// static int pid_samples_open(struct inode *inode, struct  file *filp)
// {
// 	return seq_open(filp, &pid_samples_seq_ops);
// }

// struct file_operations pid_samples_proc_fops = {
// 	.open = pid_samples_open,
// 	.llseek = seq_lseek,
// 	.read = seq_read,
// 	.release = seq_release,
// };

// static void unregister_pid_dir_proc(pid_t pid)
// {
// 	struct hlist_node *next;
// 	struct pid_proc_info *ppd;
// 	struct pmc_sample_block *psb;

// 	LCK_HASH;
// 	hash_for_each_possible_safe(pids_htable, ppd, next, node, pid) {
// 		if (*(ppd->pid) == pid) {
// 			/* Remove form htable and unlock */
// 			hash_del(&ppd->node);
// 			UCK_HASH;

// 			/* Remove the dir */
// 			proc_remove(ppd->root);

// 			/* Free the pid */
// 			vfree(ppd->pid);

// 			/* Free all stored samples */
// 			psb = ppd->dd->psb_stored;
// 			while (psb) {
// 				ppd->dd->psb_stored = ppd->dd->psb_stored->next;
// 				vfree(psb);
// 				psb = ppd->dd->psb_stored;
// 			}

// 			/* Free all available samples */
// 			psb = ppd->dd->psb;
// 			while (psb) {
// 				ppd->dd->psb = ppd->dd->psb->next;
// 				vfree(psb);
// 				psb = ppd->dd->psb;
// 			}

// 			/* Free detection struct */
// 			vfree(ppd->dd);

// 			/* Free htable elem */
// 			vfree(ppd);
// 			return;
// 		}
// 	}
// 	UCK_HASH;

// }

// /**
//  * @tgid: the tgid the thread belongs to (path: /tgid/pidp)
//  */
// void register_pid_dir_proc(pid_t *tgid, pid_t *pidp, struct detection_data *dd)
// {
// 	char name[17];
// 	struct pid_proc_info *ppd;
// 	struct proc_dir_entry *dir;

// 	if (!tgid)
// 		sprintf(name, "%d", *pidp);
// 	else
// 		sprintf(name, "%d/%d", *tgid, *pidp);

// 	ppd = vmalloc(sizeof(struct pid_proc_info));
// 	if (!ppd) return;

// 	/* create /proc/detection/<tgidp/?>1234 */
// 	dir = proc_mkdir(name, root_pd_dir);

// 	/* memory leak when releasing */
// 	proc_create_data("state", 0666, dir,
// 		 &pid_state_proc_fops, pidp);

// 	proc_create_data("samples", 0444, dir,
// 		 &pid_samples_proc_fops, dd);

// 	ppd->pid = pidp;
// 	ppd->tgid = tgid;
// 	ppd->root = dir;
// 	ppd->dd = dd;

// 	LCK_HASH;
// 	hash_add(pids_htable, &ppd->node, *ppd->pid);
// 	UCK_HASH;
// }



// static int pid_debug_level_seq_show(struct seq_file *m, void *v)
// {
// 	/** debug_level is an external variable */
// 	seq_printf(m, "%u\n", debug_level);
// 	return 0;
// }

// static int pid_debug_level_open(struct inode *inode, struct  file *filp)
// {
// 	return single_open(filp, pid_debug_level_seq_show, NULL);
// }

// static ssize_t pid_debug_level_write(struct file *filp,
// 		const char __user *buffer, size_t count, loff_t *ppos)
// {
// 	int err;
// 	unsigned val;

// 	err = kstrtouint_from_user(buffer, count, 0, &val);
// 	if (err) goto err_quiet;

// 	set_debug_pmc_detection(val);

// 	return count;
// err_quiet:
// 	return -1;
// }

// struct file_operations pid_debug_level_proc_fops = {
// 	.open = pid_debug_level_open,
// 	.read = seq_read,
// 	.write = pid_debug_level_write,
// 	.release = single_release,
// };


// static int sampling_rate_seq_show(struct seq_file *m, void *v)
// {
// 	u64 val;

// 	val = get_sampling_window();

// 	seq_printf(m, "%llx\n", val);

// 	return 0;
// }

// static int sampling_rate_open(struct inode *inode, struct  file *filp)
// {
// 	return single_open(filp, sampling_rate_seq_show, NULL);
// }

// static ssize_t sampling_rate_write(struct file *filp,
// 		const char __user *buffer_user, size_t count, loff_t *ppos)
// {
// 	// int err;
// 	u64 val;
// 	char *buffer;
// 	int err;
// 	buffer = (char*) kzalloc (sizeof(char)*count, GFP_KERNEL);
// 	err = copy_from_user((void*)buffer, (void *)buffer_user, sizeof(char)*count);

// 	// err = kstrtoull_from_user(buffer, count, 0, &val);
// 	sscanf(buffer, "%llx", &val);

// 	set_sampling_window(val);

// 	return count;
// }

// struct file_operations sampling_rate_proc_fops = {
// 	.open = sampling_rate_open,
// 	.read= seq_read,
// 	.write = sampling_rate_write,
// 	.release = single_release,
// };


// static int sampling_events_show(struct seq_file *m, void *v)
// {
// 	unsigned i;

// 	for (i = 0; i < MAX_ID_PMC; i++){
// 		seq_printf(m, "%llx\t", pmc_events[i] & 0xffff);
// 	}
// 	seq_printf(m, "\n");

// 	return 0;
// }

// static int sampling_events_open(struct inode *inode, struct  file *filp)
// {
// 	return single_open(filp, sampling_events_show, NULL);
// }

// static ssize_t sampling_events_write(struct file *filp,
// 		const char __user *buffer_user, size_t count, loff_t *ppos)
// {
// 	char *p, *buffer;
// 	// u64 sval;
// 	int err, i = 0;
// 	buffer = (char*) kzalloc (sizeof(char)*count, GFP_KERNEL);
// 	err = copy_from_user((void*)buffer, (void *)buffer_user, sizeof(char)*count);
// 	if(err) return err;
// 	while (((p = strsep(&buffer, ",")) != NULL) && i < MAX_ID_PMC) {
// 		sscanf(p, "%llx", &pmc_events[i]);
// 		i++;
// 	}
// 	while(i < MAX_ID_PMC){
// 		pmc_events[i] = 0ULL;
// 		i++;
// 	}
// 	set_sampling_events();
// 	return count;
// }

// struct file_operations sampling_events_proc_fops = {
// 	.open = sampling_events_open,
// 	.read= seq_read,
// 	.write = sampling_events_write,
// 	.release = single_release,
// };

// static int thresholds_show(struct seq_file *m, void *v)
// {
// 	seq_printf(m, "%lld\t%lld\n", tm1, tm2);

// 	return 0;
// }

// static int thresholds_open(struct inode *inode, struct  file *filp)
// {
// 	return single_open(filp, thresholds_show, NULL);
// }

// static ssize_t thresholds_write(struct file *filp,
// 		const char __user *buffer_user, size_t count, loff_t *ppos)
// {
// 	char *p, *buffer;
// 	int err;
// 	buffer = (char*) kzalloc (sizeof(char)*count, GFP_KERNEL);
// 	err = copy_from_user((void*)buffer, (void *)buffer_user, sizeof(char)*count);
// 	if(err) return err;
// 	if((p = strsep(&buffer, ",")) != NULL) sscanf(p, "%lld", &tm1);
// 	if((p = strsep(&buffer, ",")) != NULL) sscanf(p, "%lld", &tm2);
// 	return count;
// }

// struct file_operations thresholds_proc_fops = {
// 	.open = thresholds_open,
// 	.read= seq_read,
// 	.write = thresholds_write,
// 	.release = single_release,
// };

// static int attacktype_show(struct seq_file *m, void *v)
// {
// 	seq_printf(m, "%d\n", pp_mode);

// 	return 0;
// }

// static int attacktype_open(struct inode *inode, struct  file *filp)
// {
// 	return single_open(filp, attacktype_show, NULL);
// }

// static ssize_t attacktype_write(struct file *filp,
// 		const char __user *buffer, size_t count, loff_t *ppos)
// {
// 	u64 val;
// 	int err = kstrtoull_from_user(buffer, count, 0, &val);
// 	pmc_events[0] = 0x81D0;
// 	pmc_events[1] = 0x20D1;
// 	pmc_events[3] = 0x2008;
// 	if(val){
// 		tm1 = 0;
// 		tm2 = 20;
// 		pmc_events[2] = 0x8D1;
// 		pp_mode = true;
// 		set_sampling_events();
// 	}
// 	else{
// 		tm1 = 0;
// 		tm2 = 0;
// 		pmc_events[2] = 0x151;
// 		pp_mode = false;
// 		set_sampling_events();
// 	}
// 	return count;
// }

// struct file_operations attacktype_proc_fops = {
// 	.open = attacktype_open,
// 	.read= seq_read,
// 	.write = attacktype_write,
// 	.release = single_release,
// };

// static ssize_t remove_thread_write(struct file *file,
// 		const char __user *buffer, size_t count, loff_t *ppos)
// {
// 	int err;
// 	struct task_struct *ts;
// 	pid_t pid;

// 	err = kstrtoint_from_user(buffer, count, 0, &pid);
// 	if (err) {
// 		pr_info("Pid buffer err\n");
// 		goto err;
// 	}

// 	ts = get_pid_task(find_get_pid(pid), PIDTYPE_PID);

// 	/* TODO Unsafe */
// 	if (ts) {
// 		ts->detection_state &= ~PMC_D_PROFILING;
// 		put_task_struct(ts);
// 	}

// 	unregister_pid_dir_proc(pid);

// 	return count;

// // no_task:
// err:
// 	return -1;
// }

// struct file_operations remove_thread_proc_fops = {
//     .write = remove_thread_write,
// };

static ssize_t processes_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *ppos)
{
	int err;
	struct task_struct *ts;
	pid_t *pidp;

	pidp = vmalloc(sizeof(pid_t));
	if (!pidp) goto err;

	err = kstrtoint_from_user(buffer, count, 0, pidp);
	if (err) {
		pr_info("Pid buffer err\n");
		goto err;
	}

        /* Retrieve pid task_struct */
	ts = get_pid_task(find_get_pid(*pidp), PIDTYPE_PID);
	if (!ts) {
		pr_info("Cannot find task_struct for pid %u\n", *pidp);
		goto err;
	}

	// if (debug_level & DEBUG_STUFF) {
	// 	// LOOkup for the PID struct
	// 	struct task_struct *tsk = get_pid_task(find_get_pid(*pidp), PIDTYPE_PID);
	// 	if (tsk && tsk->group_leader) {
	// 		if (tsk->group_leader->pid != *pidp) {
	// 			pr_err("@DEBUG_STUFF PGID %u does not match PID %u\n", tsk->group_leader->pid, *pidp);
	// 		} else {
	// 			*pidp = tsk->group_leader->pid;
	// 		}
	// 	} else {
	// 		pr_warn("@DEBUG_STUFF PID %u error\n", *pidp);
	// 	}
	// }

	// register_pid_dir_proc(NULL, pidp, dd);

        attach_process(*pidp);

// end:
	put_task_struct(ts);
	return count;

// no_task:
err:
	return -1;
}

struct file_operations processes_proc_fops = {
    .write = processes_write,
};

// static void register_active_threads_proc(void)
// {

// 	// proc_create("detection/debug_level", 0666, NULL,
// 	// 	    &pid_debug_level_proc_fops);

// 	// proc_create("detection/sampling_rate", 0666, NULL,
// 	// 	    &sampling_rate_proc_fops);

// 	// proc_create("detection/sampling_events", 0666, NULL,
// 	// 	    &sampling_events_proc_fops);

// 	// proc_create("detection/thresholds", 0666, NULL,
// 	// 	    &thresholds_proc_fops);

// 	// proc_create("detection/attacktype", 0666, NULL,
// 	// 	    &attacktype_proc_fops);

// 	proc_create("recode/remove_thread", 0222, NULL,
// 		    &remove_thread_proc_fops);

// 	proc_create("recode/processes", 0222, NULL,
// 		    &processes_proc_fops);
// }

void init_dynamic_proc(void)
{
	root_pd_dir = proc_mkdir("recode", NULL);
	if (!root_pd_dir) {
		pr_warning("Cannot create proc entry (root) for recode module\n");
		return;
	}

	spin_lock_init(&hash_lock);

        // proc_create("recode/remove_thread", 0222, NULL,
	// 	    &remove_thread_proc_fops);

	proc_create("recode/processes", 0222, NULL,
		    &processes_proc_fops);

}

void fini_dynamic_proc(void)
{
	proc_remove(root_pd_dir);
}
