/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#include "plugins/recode_tma.h"

int register_system_hooks(void);

void unregister_system_hooks(void);

void enable_scheduler(void);

void disable_scheduler(void);

void print_tma_metrics(uint id, struct tma_profile *profile);

struct tma_profile *create_process_profile(void);

void destroy_process_profile(struct tma_profile *profile);

void aggregate_tma_profile(struct tma_profile *proc_profile,
			   struct tma_profile *group_profile);

int register_proc_group(void);

struct group_step {
	uint id;
	uint *groups_id;
	uint nr_groups;
	struct tma_profile profile;
};

extern struct group_step *gsteps;
extern uint nr_gsteps;

