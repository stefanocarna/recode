/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

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

#include "pmu.h"

/* Configure your paths */
#define PROC_TOP "pmudrv"

/* Uility */
#define _STRINGIFY(s) s
#define STRINGIFY(s) _STRINGIFY(s)
#define PATH_SEP "/"
#define GET_PATH(s) STRINGIFY(PROC_TOP) STRINGIFY(PATH_SEP) s
#define GET_SUB_PATH(p, s) STRINGIFY(p)##s

extern struct proc_dir_entry *pmu_root_pd_dir;

/* Proc and subProc functions  */
int pmu_init_proc(void);
void pmu_fini_proc(void);

// extern int register_proc_cpus(void);
int pmu_register_proc_frequency(void);
// extern int register_proc_sample_info(void);
// extern int register_proc_processes(void);
int pmu_register_proc_tma(void);
int pmu_register_proc_state(void);
int pmu_register_proc_reset(void);
int pmu_register_proc_info(void);
int pmu_register_proc_vector(void);


#endif /* _PROC_H */
