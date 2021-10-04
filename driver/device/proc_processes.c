 #include "proc.h"
 #include <linux/vmalloc.h>

/* 
 * Proc and Fops related to Tracked processes
 */

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

        attach_process(ts);

// end:
	put_task_struct(ts);
	// TODO - Release pid memory
	return count;

// no_task:
err:
	return -1;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5,6,0)
struct file_operations processes_proc_fops = {
    .write = processes_write,
#else
struct proc_ops processes_proc_fops = {
    .proc_write = processes_write,
#endif
};


int register_proc_processes(void)
{
        struct proc_dir_entry *dir;

	dir = proc_create(GET_PATH("processes"), 0222, NULL,
		    &processes_proc_fops);

        return !dir;
}