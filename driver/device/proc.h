
#ifndef _PROC_H
#define _PROC_H

#include <linux/dynamic-mitigations.h>
#include <linux/hashtable.h>
#include <linux/percpu-defs.h>
#include <linux/pid.h>
#include <linux/proc_fs.h>
#include <linux/sched/task.h>
#include <linux/seq_file.h>
#include <linux/slab.h>

#include "dependencies.h"

#include "../recode.h"

/* Configure your paths */
#define PROC_TOP "recode"

/* Uility */
#define _STRINGIFY(s) s
#define STRINGIFY(s) _STRINGIFY(s)
#define PATH_SEP "/"
#define GET_PATH(s) STRINGIFY(PROC_TOP) STRINGIFY(PATH_SEP) s
#define GET_SUB_PATH(p, s) STRINGIFY(p)##s

extern struct proc_dir_entry *root_pd_dir;

/* Proc and subProc functions  */
extern void init_proc(void);
extern void fini_proc(void);

extern int register_proc_cpus(void);
extern int register_proc_events(void);
extern int register_proc_frequency(void);
extern int register_proc_mitigations(void);
extern int register_proc_processes(void);
extern int register_proc_state(void);
extern int register_proc_statistics(void);
extern int register_proc_thresholds(void);

#endif /* _PROC_H */