
#ifndef _PROC_H
#define _PROC_H

#include <linux/hashtable.h>
#include <linux/percpu-defs.h>
#include <linux/pid.h>
#include <linux/proc_fs.h>
#include <linux/sched/task.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/version.h>

#include "recode.h"

/* Configure your paths */
#define PROC_TOP "recode"

/* Uility */
#define _STRINGIFY(s) s
#define STRINGIFY(s) _STRINGIFY(s)
#define PATH_SEP "/"
#define GET_PATH(s) STRINGIFY(PROC_TOP) STRINGIFY(PATH_SEP) s
#define GET_SUB_PATH(p, s) STRINGIFY(p)##s

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 6, 0)
#define ONLY_OPEN_PROC_FOPS(name, open)                                        \
	struct file_operations name##_proc_fops = {                            \
		.open = open,                                                  \
		.read = seq_read,                                              \
		.llseek = seq_lseek,                                           \
		.release = single_release,                                     \
	}

#define ONLY_WRITE_PROC_FOPS(name, write)                                      \
	struct file_operations name##_proc_fops = {                            \
		.open = nonseekable_open,                                      \
		.write = write,                                                \
		.llseek = no_llseek,                                           \
	}

#define SIMPLE_RD_WR_PROC_FOPS(name, open, write)                              \
	struct file_operations name##_proc_fops = {                            \
		.open = open,                                                  \
		.read = seq_read,                                              \
		.llseek = seq_lseek,                                           \
		.write = write,                                                \
		.release = single_release,                                     \
	}
#else
#define ONLY_OPEN_PROC_FOPS(name, open)                                        \
	struct proc_ops name##_proc_fops = {                                   \
		.proc_open = open,                                             \
		.proc_read = seq_read,                                         \
		.proc_lseek = seq_lseek,                                       \
		.proc_release = single_release,                                \
	}
#define ONLY_WRITE_PROC_FOPS(name, write)                                      \
	struct proc_ops name##_proc_fops = {                                   \
		.proc_open = nonseekable_open,                                 \
		.proc_write = write,                                           \
		.proc_lseek = no_llseek,                                       \
	}

#define SIMPLE_RD_WR_PROC_FOPS(name, open, write)                              \
	struct proc_ops name##_proc_fops = {                                   \
		.proc_open = open,                                             \
		.proc_read = seq_read,                                         \
		.proc_lseek = seq_lseek,                                       \
		.proc_write = write,                                           \
		.proc_release = single_release,                                \
	}
#endif

extern struct proc_dir_entry *root_pd_dir;

/* Proc and subProc functions  */
extern int recode_init_proc(void);
extern void recode_fini_proc(void);

extern int rf_after_proc_init(void);
extern void rf_before_proc_fini(void);

extern int recode_register_proc_cpus(void);
// extern int register_proc_frequency(void);
// extern int register_proc_sample_info(void);
extern int recode_register_proc_processes(void);
extern int recode_register_proc_state(void);
extern int recode_register_proc_groups(void);

#ifdef SECURITY_MODULE_ON
extern int recode_register_proc_mitigations(void);
extern int recode_register_proc_thresholds(void);
extern int recode_register_proc_security(void);
extern int recode_register_proc_statistics(void);
#endif

#endif /* _PROC_H */