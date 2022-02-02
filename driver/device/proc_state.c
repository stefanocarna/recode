 #include "proc.h"

/*
 * Proc and Fops related to Recode state
 */

static ssize_t state_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *ppos)
{
	int err;
	unsigned state;

	err = kstrtouint_from_user(buffer, count, 0, &state);
	if (err) {
		pr_info("Pid buffer err\n");
		goto err;
	}

	recode_set_state(state);

	return count;
err:
	return -1;
}

struct file_operations state_proc_fops = {
    .write = state_write,
};


int register_proc_state(void)
{
        struct proc_dir_entry *dir;

	dir = proc_create(GET_PATH("state"), 0666, NULL,
		    &state_proc_fops);

        return !dir;
}
