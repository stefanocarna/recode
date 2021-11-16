#include <linux/sched.h>
#include <linux/cpumask.h>
#include <linux/mm.h>
#include <linux/hashtable.h>
#include <linux/slab.h>
#include <linux/wait.h>

#include <linux/sched/signal.h>

#include "recode.h"

/* PIDs are stored into a Hashmap */
DECLARE_HASHTABLE(tracked_map, 8);
/* Lock the list access */
static spinlock_t lock;

struct tp_node {
	pid_t id;
	struct hlist_node node;
	void *payload;
};

int recode_groups_init(void)
{
	int err = 0;

	spin_lock_init(&lock);

	hash_init(tracked_map);

	return err;
}

/* Delete all the groups */
void recode_groups_fini(void)
{
	unsigned long flags;
	uint groups = 0;
	uint bkt;
	struct tp_node *cur;
	struct hlist_node *next;

	spin_lock_irqsave(&lock, flags);
	hash_for_each_safe(tracked_map, bkt, next, cur, node) {
		hash_del(&cur->node);
		kvfree(cur);
		groups++;
	}
	spin_unlock_irqrestore(&lock, flags);
	pr_info("Destroyed %u groups\n", groups);
}

void *create_group(struct task_struct *task, size_t payload_size)
{
	unsigned long flags;
	struct tp_node *tp;
	pid_t id = task->tgid;

	tp = kvzalloc(sizeof(struct tp_node) + payload_size, GFP_KERNEL);
	if (!tp)
		return NULL;

	tp->id = id;

	spin_lock_irqsave(&lock, flags);
	hash_add(tracked_map, &tp->node, id);
	spin_unlock_irqrestore(&lock, flags);

	send_sig_info(SIGSTOP, SEND_SIG_NOINFO, task);

	pr_info("Created group %u\n", id);
	return tp->payload;
}

void *has_group_payload(struct task_struct *task)
{
	unsigned long flags;
	struct tp_node *cur;
	pid_t id = task->tgid;

	spin_lock_irqsave(&lock, flags);
	hash_for_each_possible(tracked_map, cur, node, id) {
		if (cur->id == id) {
			spin_unlock_irqrestore(&lock, flags);
			return cur->payload;
		}
	}
	spin_unlock_irqrestore(&lock, flags);
	return NULL;
}


bool is_group_creator(struct task_struct *task)
{
	if (task->pid != task->tgid)
		return false;

	return !!has_group_payload(task);
}

void destroy_group(struct task_struct *task)
{
	unsigned long flags;
	bool exist = false;
	struct tp_node *cur;
	struct hlist_node *next;
	pid_t id = task->tgid;

	spin_lock_irqsave(&lock, flags);
	hash_for_each_possible_safe(tracked_map, cur, next, node, id) {
		if (cur->id == id) {
			exist = true;
			hash_del(&cur->node);
			break;
		}
	}
	spin_unlock_irqrestore(&lock, flags);

	if (exist) {
		kvfree(cur);
		pr_info("Destroyed group %u\n", id);
	}
}
