/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef _RECODE_GROUPS_H
#define _RECODE_GROUPS_H

struct group_entity {
	char name[TASK_COMM_LEN];
	uint id;
	void *data;
	spinlock_t lock;

	/* Atomicity is not required */
	uint nr_processes;
	struct list_head p_list;
	bool profiling;

	struct group_stats stats;
	bool active;

	/* TODO Remove */
	u64 utime;
	u64 stime;
	int nr_active_tasks;
};

struct proc_entity {
	pid_t pid;
	void *data;
	struct task_struct *task;
	struct group_entity *group;

	struct process_stats stats;
	/* Stats data */
	u64 utime_snap;
	u64 stime_snap;
};

struct group_node {
	uint key;
	struct group_entity *group;
	struct hlist_node node;
};

struct proc_node {
	uint key;
	struct proc_entity *proc;
	struct hlist_node node;
};

struct proc_list {
	uint key;
	struct proc_entity *proc;
	struct list_head list;
};

int recode_groups_init(void);
void recode_groups_fini(void);

int register_process_to_group(pid_t pid, struct group_entity *group,
			      void *data);

void *unregister_process_from_group(pid_t pid, struct group_entity *group);

struct group_entity *create_group(char *gname, uint id, void *payload);

struct proc_entity *get_proc_by_pid(pid_t pid);
struct group_entity *get_group_by_proc(pid_t pid);
struct group_entity *get_group_by_id(uint id);
struct group_entity *get_next_group_by_id(uint id);

void *destroy_group(uint id);
void set_group_active(struct group_entity *gentity, bool active);

void signal_to_group_by_id(uint signal, uint id);
void signal_to_group(uint signal, struct group_entity *group);
void signal_to_all_groups(uint signal);
void schedule_all_groups(void);
void start_group_stats(struct group_entity *group);
void stop_group_stats(struct group_entity *group);

#endif /* _RECODE_GROUPS_H */