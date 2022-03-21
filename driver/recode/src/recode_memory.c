#include <linux/vmalloc.h>

#include "recode_memory.h"

DEFINE_PER_CPU(struct memory_bulk_manager *, pcpu_memory_bulk_manager);

struct memory_bulk_manager *init_memory_bulk_menager(uint cpu)
{
	struct memory_bulk_manager *mbm = NULL;

	// We use a flexbile array to generate p_s_rings
	mbm = vmalloc(sizeof(*mbm));
	if (!mbm)
		goto err;

	mbm->head = vzalloc(sizeof(*mbm->head) + BULK_MEMORY_SIZE);
	if (!mbm->head)
		goto no_head;

	mbm->cpu = cpu;
	mbm->cur = mbm->head;
	mbm->cur->size = BULK_MEMORY_SIZE;

	return mbm;
no_head:
	vfree(mbm);
err:
	return NULL;
}

void fini_memory_bulk_menager(uint cpu)
{
	vfree(per_cpu(pcpu_memory_bulk_manager, cpu));
}

u8 *get_from_memory_bulk_local(size_t amount)
{
	uint offset;
	struct memory_bulk *mb = this_cpu_read(pcpu_memory_bulk_manager)->cur;

	// TODO Alloc memory
	if (mb->free < amount)
		return NULL;

	offset = mb->used;
	mb->used += amount;
	mb->free -= amount;

	return mb->raw_memory + offset;
}
