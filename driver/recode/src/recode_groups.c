#include <linux/sched.h>
#include <linux/cpumask.h>
#include <linux/mm.h>
#include <linux/hashtable.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/pid.h>
#include <linux/sched.h>

#include <linux/sched/signal.h>

#include "recode.h"
#include "recode_groups.h"

uint nr_groups;

/* PIDs are stored into a Hashmap */
static DECLARE_HASHTABLE(proc_map, 8);
static DECLARE_HASHTABLE(group_map, 8);
/* Lock the list access */
static spinlock_t p_lock;
static spinlock_t g_lock;

int recode_groups_init(void)
{
	int err = 0;

	spin_lock_init(&p_lock);
	spin_lock_init(&g_lock);

	hash_init(proc_map);
	hash_init(group_map);

	return err;
}

static void free_group_proc_list(struct group_entity *group)
{
	unsigned long flags;
	struct proc_list *cur, *tmp;

	spin_lock_irqsave(&group->lock, flags);
	list_for_each_entry_safe(cur, tmp, &group->p_list, list) {
		list_del(&cur->list);
		kfree(cur);
	}
	spin_unlock_irqrestore(&group->lock, flags);

	group->nr_processes = 0;
}

/* Clear all maps */
/* NOTE We cannot free enitities here */
void recode_groups_fini(void)
{
	unsigned long flags;
	uint nr_del_procs = 0;
	uint nr_del_groups = 0;
	uint bkt;
	struct proc_node *pcur;
	struct group_node *gcur;
	struct hlist_node *next;

	spin_lock_irqsave(&g_lock, flags);
	hash_for_each_safe(group_map, bkt, next, gcur, node) {
		free_group_proc_list(gcur->group);
		hash_del(&gcur->node);
		kfree(gcur);
		nr_groups--;
		nr_del_groups++;
	}
	spin_unlock_irqrestore(&g_lock, flags);

	spin_lock_irqsave(&p_lock, flags);
	hash_for_each_safe(proc_map, bkt, next, pcur, node) {
		hash_del(&pcur->node);
		kfree(pcur);
		nr_del_procs++;
	}
	spin_unlock_irqrestore(&p_lock, flags);

	pr_info("Proc Entities leak (%u)\n", nr_del_procs);
	pr_info("Groups (%u) destroyed (%u)\n", nr_groups, nr_del_groups);
}

static int insert_process_to_group_list(pid_t pid, struct group_entity *group,
					struct proc_entity *pentity)
{
	unsigned long flags;
	struct proc_list *p_list = kmalloc(sizeof(*p_list), GFP_KERNEL);

	if (!p_list)
		return -ENOMEM;

	p_list->key = pid;
	p_list->proc = pentity;

	spin_lock_irqsave(&group->lock, flags);
	list_add_tail(&p_list->list, &group->p_list);
	group->nr_processes++;
	spin_unlock_irqrestore(&group->lock, flags);

	pr_info("[G] Add #p %u @g %u (%u)\n", pid, group->id,
		group->nr_processes);
	return 0;
}

static struct proc_entity *
remove_process_from_group_list(pid_t pid, struct group_entity *group)
{
	bool exist = false;
	unsigned long flags;
	struct proc_list *cur, *tmp;
	struct proc_entity *pentity = NULL;

	spin_lock_irqsave(&group->lock, flags);
	list_for_each_entry_safe(cur, tmp, &group->p_list, list)
		if (cur->key == pid) {
			exist = true;
			list_del(&cur->list);
			group->nr_processes--;
			break;
		}
	spin_unlock_irqrestore(&group->lock, flags);

	if (exist) {
		pr_info("[G] Del #p %u @g %u (%u)\n", pid, group->id,
			group->nr_processes);
		pentity = cur->proc;
		kfree(cur);
	}

	return pentity;
}

static int insert_process_to_processes_map(struct proc_entity *pentity)
{
	unsigned long flags;
	struct proc_node *pnode;

	pnode = kmalloc(sizeof(struct proc_node), GFP_KERNEL);
	if (!pnode)
		return -ENOMEM;

	pnode->key = pentity->pid;
	pnode->proc = pentity;

	spin_lock_irqsave(&p_lock, flags);
	hash_add(proc_map, &pnode->node, pnode->key);
	spin_unlock_irqrestore(&p_lock, flags);

	pr_info("[P] Add #p %u @g %u (%u)\n", pentity->pid, pentity->group->id,
		 pentity->group->nr_processes);

	return 0;
}

void remove_process_from_processes_map(pid_t pid, struct group_entity *group)
{
	bool exist = false;
	unsigned long flags;
	struct proc_node *cur;
	struct hlist_node *next;

	spin_lock_irqsave(&p_lock, flags);
	hash_for_each_possible_safe(proc_map, cur, next, node, pid) {
		if (cur->key == pid) {
			exist = true;
			hash_del(&cur->node);
			break;
		}
	}
	spin_unlock_irqrestore(&p_lock, flags);

	if (exist) {
		kvfree(cur);
		pr_debug("[P] Del #p %u @g %u (%u)\n", pid, group->id,
			 group->nr_processes);
	}
}

int register_process_to_group(pid_t pid, struct group_entity *group, void *data)
{
	int err = 0;
	struct proc_entity *pentity;

	if (!group)
		return -EINVAL;

	pentity = kzalloc(sizeof(struct group_entity), GFP_KERNEL);
	if (!pentity)
		return -ENOMEM;

	/* TODO we may save this call by passing the task_struct as parameter */
	pentity->task = get_pid_task(find_get_pid(pid), PIDTYPE_PID);

	if (!pentity->task) {
		err = -ESRCH;
		goto no_task;
	}

	pentity->pid = pid;
	pentity->data = data;
	pentity->group = group;

	err = insert_process_to_group_list(pid, group, pentity);
	if (err)
		goto err_group;

	err = insert_process_to_processes_map(pentity);
	if (err)
		goto err_process;

	return 0;

err_process:
	remove_process_from_group_list(pid, group);
err_group:
	put_task_struct(pentity->task);
no_task:
	kfree(pentity);
	return err;
}

void *unregister_process_from_group(pid_t pid, struct group_entity *group)
{
	void *data = NULL;
	struct proc_entity *pentity;
	unsigned long flags;

	if (!group)
		return NULL;

	remove_process_from_processes_map(pid, group);

	pentity = remove_process_from_group_list(pid, group);

	spin_lock_irqsave(&group->lock, flags);
	/* Update stats */
	if (group->profiling) {
		pentity->utime_snap =
			pentity->task->utime - pentity->utime_snap;
		pentity->stime_snap =
			pentity->task->stime - pentity->stime_snap;
		group->utime += pentity->utime_snap;
		group->stime += pentity->stime_snap;
		if (pentity->utime_snap || pentity->stime_snap)
			pentity->group->nr_active_tasks++;
	}
	spin_unlock_irqrestore(&group->lock, flags);

	data = pentity->data;
	put_task_struct(pentity->task);
	kfree(pentity);

	return data;
}

struct group_entity *create_group(char *gname, uint id, void *data)
{
	unsigned long flags;
	struct group_node *gnode;
	struct group_entity *gentity;

	gnode = kzalloc(sizeof(struct group_node), GFP_KERNEL);
	if (!gnode)
		return NULL;

	gentity = kzalloc(sizeof(struct group_entity), GFP_KERNEL);
	if (!gentity)
		goto no_entity;

	gnode->key = id;
	gnode->group = gentity;

	gentity->id = id;
	gentity->data = data;
	gentity->nr_processes = 0;

	memcpy(gentity->name, gname, TASK_COMM_LEN);
	gentity->name[TASK_COMM_LEN - 1] = '\0';

	INIT_LIST_HEAD(&gentity->p_list);
	spin_lock_init(&gentity->lock);

	spin_lock_irqsave(&g_lock, flags);
	hash_add(group_map, &gnode->node, id);
	nr_groups++;
	spin_unlock_irqrestore(&g_lock, flags);

	pr_info("Created group %u [%s]\n", id, gname);
	return gentity;

no_entity:
	kfree(gnode);
	return NULL;
}

void *destroy_group(uint id)
{
	void *data = NULL;
	bool exist = false;
	unsigned long flags;
	struct group_node *cur;
	struct hlist_node *next;

	spin_lock_irqsave(&g_lock, flags);
	hash_for_each_possible_safe(group_map, cur, next, node, id) {
		if (cur->key == id) {
			exist = true;
			hash_del(&cur->node);
			nr_groups--;
			break;
		}
	}
	spin_unlock_irqrestore(&g_lock, flags);

	if (exist) {
		data = cur->group->data;
		pr_info("Destroyed group %u - %s\n", id, cur->group->name);
		kfree(cur->group);
		kfree(cur);
	}

	return data;
}

void set_group_active(struct group_entity *gentity, bool active)
{
	if (!gentity)
		return;

	if (gentity->active == active)
		return;

	gentity->active = active;

	signal_to_group(active ? SIGCONT : SIGSTOP, gentity);
}

struct proc_entity *get_proc_by_pid(pid_t pid)
{
	unsigned long flags;
	struct proc_node *cur;

	spin_lock_irqsave(&p_lock, flags);
	hash_for_each_possible(proc_map, cur, node, pid) {
		if (cur->key == pid) {
			spin_unlock_irqrestore(&p_lock, flags);
			return cur->proc;
		}
	}
	spin_unlock_irqrestore(&p_lock, flags);
	return NULL;
}

struct group_entity *get_group_by_proc(pid_t pid)
{
	struct proc_entity *pentity;

	pentity = get_proc_by_pid(pid);

	return pentity ? pentity->group : NULL;
}

struct group_entity *get_group_by_id(uint id)
{
	unsigned long flags;
	struct group_node *cur;

	spin_lock_irqsave(&g_lock, flags);
	hash_for_each_possible(group_map, cur, node, id) {
		if (cur->key == id) {
			spin_unlock_irqrestore(&g_lock, flags);
			return cur->group;
		}
	}
	spin_unlock_irqrestore(&g_lock, flags);
	return NULL;
}

struct group_entity *get_next_group_by_id(uint id)
{
	uint bkt;
	unsigned long flags;
	struct group_node *cur = NULL;
	bool stop = !id || nr_groups < 2;

	spin_lock_irqsave(&g_lock, flags);

	if (!nr_groups) {
		spin_unlock_irqrestore(&g_lock, flags);
		return NULL;
	}

	hash_for_each(group_map, bkt, cur, node) {
		pr_debug("%s GROUP %u\n", __func__, cur->key);
		if (stop)
			break;
		if (cur->key == id)
			stop = true;
	}
	spin_unlock_irqrestore(&g_lock, flags);

	if (!cur && nr_groups > 1)
		return get_next_group_by_id(0);

	return cur->group;
}

void start_group_stats(struct group_entity *group)
{
	struct proc_list *cur;
	struct proc_entity *proc;
	unsigned long flags;

	spin_lock_irqsave(&group->lock, flags);

	group->utime = 0;
	group->stime = 0;
	group->nr_active_tasks = 0;

	list_for_each_entry(cur, &group->p_list, list) {
		proc = cur->proc;
		/* TODO Change */
		proc->utime_snap = proc->task->utime;
		proc->stime_snap = proc->task->stime;
		// proc->group->nr_active_tasks = 0;

		pr_debug("[%s] *-* P[%u] - uTIME: %llu, sTIME: %llu\n",
			 group->name, proc->pid, proc->utime_snap,
			 proc->stime_snap);
	}

	group->profiling = true;

	spin_unlock_irqrestore(&group->lock, flags);
}

void stop_group_stats(struct group_entity *group)
{
	struct proc_list *cur;
	struct proc_entity *proc;
	unsigned long flags;

	spin_lock_irqsave(&group->lock, flags);
	list_for_each_entry(cur, &group->p_list, list) {
		proc = cur->proc;
		proc->utime_snap = proc->task->utime - proc->utime_snap;
		proc->stime_snap = proc->task->stime - proc->stime_snap;
		/* TODO RESET */
		proc->group->utime += proc->utime_snap;
		proc->group->stime += proc->stime_snap;
		if (proc->utime_snap || proc->stime_snap)
			proc->group->nr_active_tasks++;
		pr_debug(
			"[%s] uTIME: %llu, sTIME: %llu <-- P[%u] - uTIME: %llu, sTIME: %llu\n",
			group->name, proc->group->utime, proc->group->stime,
			proc->pid, proc->utime_snap, proc->stime_snap);
	}

	group->profiling = false;

	spin_unlock_irqrestore(&group->lock, flags);
}

static void __signal_to_group(uint signal, struct group_entity *group)
{
	struct proc_list *cur;
	unsigned long flags;

	spin_lock_irqsave(&group->lock, flags);
	list_for_each_entry(cur, &group->p_list, list)
		send_sig_info(signal, SEND_SIG_NOINFO, cur->proc->task);
	spin_unlock_irqrestore(&group->lock, flags);
}

void signal_to_group(uint signal, struct group_entity *group)
{
	if (!group)
		return;

	__signal_to_group(signal, group);
}

void signal_to_group_by_id(uint signal, uint id)
{
	struct group_entity *group = get_group_by_id(id);

	if (!group)
		return;

	__signal_to_group(signal, group);
}

void signal_to_all_groups(uint signal)
{
	uint bkt;
	unsigned long flags;
	struct group_node *cur;

	spin_lock_irqsave(&g_lock, flags);
	hash_for_each(group_map, bkt, cur, node)
		__signal_to_group(signal, cur->group);
	spin_unlock_irqrestore(&g_lock, flags);
}
