#include <linux/vmalloc.h>

#include "recode.h"

DEFINE_PER_CPU(struct statistic, pcpu_statistics) = { 0 };

bool push_ps_ring(struct pmcs_snapshot_chain *chain,
                             struct pmcs_snapshot_ring *ring)
{
	unsigned long flags;
        if (!ring)
                return false;

        pr_debug("%p] PUSHING CHAIN NEXT %p\n", chain, ring);
	
        spin_lock_irqsave(&chain->lock, flags);
        /* Reset IDX */
        ring->idx = 0;
        
	if (chain->head == NULL) {
                chain->head = ring;
        } else {
                chain->tail->next = ring;
        }
        chain->tail = ring;
        /* Unlink a potential recycled ring */
        chain->tail->next = NULL;
	spin_unlock_irqrestore(&chain->lock, flags);
        return true;
}

struct pmcs_snapshot_ring *pop_ps_ring(struct pmcs_snapshot_chain *chain)
{
	unsigned long flags;
        struct pmcs_snapshot_ring *elem = NULL;
	spin_lock_irqsave(&chain->lock, flags);
	if (chain->head != NULL) {
                elem = chain->head;
                chain->head = chain->head->next;
        }
        pr_debug("%p] POPPING CHAIN NEXT %p\n", chain, elem ? elem->next : NULL);
	spin_unlock_irqrestore(&chain->lock, flags);
        return elem;
}

void fake_log_write(struct pmc_logger *logger)
{
        unsigned k;
        struct pmcs_snapshot *ps = vzalloc(sizeof(struct pmcs_snapshot));
        if (!ps) {
                pr_warn("Cannot allocate pmcs_snapshot\n");
                return;
        }

        for (k = 0; k < 120; ++k)
                write_log_sample(logger, ps);
}

struct pmc_logger *init_logger(unsigned cpu)
{
        unsigned i;
        struct pmc_logger *logger;

        // We use a flexbile array to generate p_s_rings
        logger = vzalloc(sizeof(struct pmc_logger));
        if (!logger) 
		return NULL;

        logger->chain.head = vmalloc(RING_COUNT * RING_SIZE);
        if (!logger->chain.head) {
                vfree(logger);
		return NULL;
        } 

        spin_lock_init(&logger->chain.lock);
        spin_lock_init(&logger->wr.lock);
        spin_lock_init(&logger->rd.lock);


        /* Shortcut to free rings */
        logger->ptr = logger->chain.head;

	if (cpu == 0) {
                pr_debug("Created and linked %u rings\n", RING_COUNT);
	}

        /* Link all rings */
        for (i = 0; i < RING_COUNT - 1; ++i) {
                logger->chain.head[i].idx = 0;
	        logger->chain.head[i].length = RING_BUFF_LENGTH;
                logger->chain.head[i].next = &logger->chain.head[i+1];

                if (cpu == 0) {
        		pr_debug("LINKING %u: %p [%u, %u]\n", i ,&logger->chain.head[i], logger->chain.head[i].idx, logger->chain.head[i].length);
                }

                if (i == RING_COUNT - 2) {
                        logger->chain.tail = &logger->chain.head[i+1];
                        logger->chain.tail->idx = 0;
                        logger->chain.tail->length = RING_BUFF_LENGTH;
                        logger->chain.tail->next = NULL;
                }
        }

        if (cpu == 0) {
                pr_debug("Created and linked %u rings\n", RING_COUNT);
        }

        /* Setup the readable rings */
        // logger->rd.head = NULL;
        // logger->rd.tail = NULL;
        /* Setup the writable ring by popping the first element */
        push_ps_ring(&logger->wr, pop_ps_ring(&logger->chain));

        /* Unlink the element */
	logger->cpu = cpu;

        // if (cpu == 0)
        //         fake_log_write(logger);

        return logger;
}

void reset_logger(struct pmc_logger *logger)
{
        if (!logger)
                return;
                
        logger->count = 0;
}

// static void free_pmcs_snapshot_ring(struct pmcs_snapshot_ring *ring)
// {
//         struct pmcs_snapshot_ring *tmp;
//         while(ring) {
//                 tmp = ring;
//                 ring = ring->next;
//                 vfree(tmp);
//         }
// }

void fini_logger(struct pmc_logger *logger)
{
        /* Enable if individually allocated */
        // free_pmcs_snapshot_ring(logger->chain);
        // free_pmcs_snapshot_ring(logger->wr_buff);
        // free_pmcs_snapshot_ring(logger->rd_buff);
        vfree(logger->ptr);
        vfree(logger);
}

int write_log_sample(struct pmc_logger *logger, struct pmcs_snapshot *sample)
{
        struct pmcs_snapshot_ring *psr;
        
        if (WARN_ONCE(!logger, "WRITE Internal error, logger is NULL\n")) {
                return -1;
        }

        // TODO Remove
        if (logger->cpu != 0)
                return 0;
                
        psr = logger->wr.head;
        
        if (WARN_ONCE(!psr, "Internal error, wr_buff is NULL\n")) {
                return -1;
        }

        if(psr->idx >= psr->length) {
                // pr_info("* Before RD push %p\n", logger->wr.head);
                push_ps_ring(&logger->rd, pop_ps_ring(&logger->wr));
                // pr_info("** Before WR push %p\n", logger->wr.head);
                push_ps_ring(&logger->wr, pop_ps_ring(&logger->chain));
                // pr_info("*** After WR push %p\n", logger->wr.head);

                // TODO Optimize
                if (WARN_ONCE(!logger->wr.head, "Cannot log on cpu %u, wr_buff is FULL\n", logger->cpu)) {
                        return -1;
                } else {
                        return write_log_sample(logger, sample);
                }
        }

        if (logger->count % 8 == 0) {
                pr_debug("Wrote %u samples on core %u\n",
                        logger->count, logger->cpu);
        }

        memcpy(&psr->buff[psr->idx], sample, sizeof(struct pmcs_snapshot));

        psr->idx++;
        logger->count++;
        return 0;
}

struct pmcs_snapshot *read_log_sample(struct pmc_logger *logger)
{
        struct pmcs_snapshot *sample;
        struct pmcs_snapshot_ring *psr;
        
        if (WARN_ONCE(!logger, "READ Internal error, logger is NULL\n")) {
                return NULL;
        }

        psr = logger->rd.head;
        
        if (WARN_ONCE(!psr, "Internal error, rd_buff is NULL\n")) {
                return NULL;
        }

        if(psr->idx >= psr->length) {
                push_ps_ring(&logger->chain, pop_ps_ring(&logger->rd));

                // TODO Optimize
                if (WARN_ONCE(!logger->rd.head, "Cannot log on cpu %u, rd_buff is EMPTY\n", logger->cpu)) {
                        return NULL;
                } else {
                        return read_log_sample(logger);
                }
        }

        /* TODO It may requires to create a copy */
        sample = &psr->buff[psr->idx];

        psr->idx++;
        return sample;
}
