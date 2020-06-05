#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <asm/desc.h>

#include "idt_patcher.h"

static struct desc_ptr bkp_idtr = {0, 0};

#define MAX_PATCHED_IDT 2
#define IDX_STEP	1U

static DEFINE_SPINLOCK(idt_modify_lock);

static struct desc_ptr *patched_idt[MAX_PATCHED_IDT];
static unsigned patched_idt_idx = 0;

static void smp_load_idt(void *addr)
{
	struct desc_ptr *idtr;
	idtr = (struct desc_ptr *) addr;
	load_idt(idtr);
	pr_info("[CPU %u]: loaded IDT at %lx\n", smp_processor_id(), idtr->address);
}

static struct desc_ptr *clone_current_idt(void)
{
	struct desc_ptr *idtr;
	void *idt_table;

	idtr = (struct desc_ptr *)kmalloc(sizeof(struct desc_ptr), GFP_KERNEL);
	if (!idtr) return 0;

	idt_table = (void *)__get_free_page(GFP_KERNEL);
	if (!idt_table) return 0;

	store_idt(idtr);

	memcpy(idt_table, (void *)(idtr->address), PAGE_SIZE);

	idtr->address = (unsigned long) idt_table;
	return idtr;
}

/*
 * Assuming there is a unique IDT instance shared among cores 
 */
static int backup_system_idt(void)
{
	if (bkp_idtr.size > 0) {
		pr_warn("IDT already backed up\n");
	} else {
		store_idt(&bkp_idtr);
		pr_info("[BACKUP CREATED] System IDT found at %lx\n", bkp_idtr.address);
	}
	return 0;
}

static void restore_system_idt(void)
{
	if (bkp_idtr.size > 0) {
		on_each_cpu(smp_load_idt, &bkp_idtr, 1);
		pr_info("original IDT restored\n");
	} else {
		pr_warn("IDT backup not found\n");
	}
}

int idt_patcher_install_entry(unsigned long handler, unsigned vector, unsigned dpl)
{
	gate_desc new_gate;
	gate_desc *new_idt, *cur_idt;
	unsigned long flags;

	spin_lock_irqsave(&idt_modify_lock, flags);
	cur_idt = (gate_desc *)patched_idt[patched_idt_idx]->address;
	patched_idt_idx ^= IDX_STEP;
	new_idt = (gate_desc *)patched_idt[patched_idt_idx]->address;

	// TODO pr_warn & return
	if ((vector >> 8) || dpl > 3) {
		pr_info("Invalid vector %x or dpl %x\n", vector, dpl);
	}

	// Create the entry in the spare IDT instance
	pack_gate(&new_gate, GATE_INTERRUPT, handler, dpl, 0, 0);
	write_idt_entry(new_idt, vector, &new_gate);

	// Set the spare IDT instance as system's one
	on_each_cpu(smp_load_idt, patched_idt[patched_idt_idx], 1);

	// Upload old IDT instance	
	write_idt_entry(cur_idt, vector, &new_gate);

	spin_unlock_irqrestore(&idt_modify_lock, flags);
	return 0;
}

EXPORT_SYMBOL(idt_patcher_install_entry);

static void idt_instance_free(struct desc_ptr *idtr)
{
	if (!idtr) return;
	free_page((unsigned long) idtr->address);
	kfree(idtr);
}

static __init int idt_patcher_module_init(void)
{
	unsigned i = MAX_PATCHED_IDT;
	
	// Create spare IDT instances
	while(i--) {
		patched_idt[i] = clone_current_idt();
		if (!patched_idt[i]) goto no_mem;
	}


	// Backup System IDT
	backup_system_idt();

	// Load the first spare IDT instance as system's one
	on_each_cpu(smp_load_idt, patched_idt[patched_idt_idx], 1);

	pr_info("IDT_PATCHER module loaded\n");
	return 0;
no_mem:
	while(i++ < MAX_PATCHED_IDT) {
		idt_instance_free(patched_idt[i]);
	}

	pr_err("Cannot allocate memory while loading the module\n");
	return -ENOMEM;
}

static void __exit idt_patcher_module_exit(void)
{
	unsigned i = MAX_PATCHED_IDT;

	// Restore System IDT
	restore_system_idt();

	while(i--) {
		idt_instance_free(patched_idt[i]);
	}

	pr_info("IDT_PATCHER module shutdown\n");
}

module_init(idt_patcher_module_init);
module_exit(idt_patcher_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Stefano Carna'");