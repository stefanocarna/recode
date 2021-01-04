#include "proc.h"

static spinlock_t hash_lock;
#define LCK_HASH spin_lock(&hash_lock)
#define UCK_HASH spin_unlock(&hash_lock)

struct proc_dir_entry *root_pd_dir;

void init_proc(void)
{
	root_pd_dir = proc_mkdir(PROC_TOP, NULL);

	if (!root_pd_dir) {
		pr_warning("Cannot create proc entry (root) for recode module\n");
		return;
	}

	spin_lock_init(&hash_lock);

	register_proc_cpus();
	register_proc_events();
	register_proc_frequency();
	register_proc_processes();
	register_proc_state();
	register_proc_thresholds();
}

void fini_proc(void)
{
	proc_remove(root_pd_dir);
}
