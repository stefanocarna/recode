#include <linux/vmalloc.h>

#include "recode.h"
#include "pmu/pmi.h"
#include "recode_collector.h"

atomic_t on_samples_flushing = ATOMIC_INIT(0);

bool push_ps_ring(struct data_logger_chain *chain,
                             struct data_logger_ring *ring)
{
	unsigned long flags;
        if (!ring)
                return false;

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

bool push_ps_ring_reset(struct data_logger_chain *chain,
                             struct data_logger_ring *ring)
{
        if (!ring)
                return false;
        
        ring->length = RING_BUFF_LENGTH;
        return push_ps_ring(chain, ring);
}

struct data_logger_ring *pop_ps_ring(struct data_logger_chain *chain)
{
	unsigned long flags;
        struct data_logger_ring *elem = NULL;
	spin_lock_irqsave(&chain->lock, flags);
	if (chain->head != NULL) {
                elem = chain->head;
                chain->head = chain->head->next;
        }
        // pr_debug("%p] POPPING CHAIN NEXT %p\n", chain, elem ? elem->next : NULL);
	spin_unlock_irqrestore(&chain->lock, flags);
        return elem;
}

struct data_logger *init_logger(unsigned cpu)
{
        unsigned i;
        struct data_logger *logger;

        // We use a flexbile array to generate p_s_rings
        logger = vzalloc(sizeof(struct data_logger));
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

        pr_debug("[%u] Created and linked %u rings\n", cpu, RING_COUNT);

        /* Link all rings */
        for (i = 0; i < RING_COUNT - 1; ++i) {
                logger->chain.head[i].idx = 0;
	        logger->chain.head[i].length = RING_BUFF_LENGTH;
                logger->chain.head[i].next = &logger->chain.head[i+1];

		pr_debug("[%u] LINKING %u: %p [%u, %u]\n", i , cpu,
				&logger->chain.head[i],
				logger->chain.head[i].idx,
				logger->chain.head[i].length);

                if (i == RING_COUNT - 2) {
                        logger->chain.tail = &logger->chain.head[i+1];
                        logger->chain.tail->idx = 0;
                        logger->chain.tail->length = RING_BUFF_LENGTH;
                        logger->chain.tail->next = NULL;
                }
        }

	pr_debug("[%u] Created and linked %u rings\n", cpu, RING_COUNT);

        /* Setup the readable rings */
        // logger->rd.head = NULL;
        // logger->rd.tail = NULL;
        /* Setup the writable ring by popping the first element */
        push_ps_ring(&logger->wr, pop_ps_ring(&logger->chain));

        /* Unlink the element */
	logger->cpu = cpu;

        return logger;
}

void reset_logger(struct data_logger *logger)
{
        struct data_logger_ring *psr;

        if (!logger)
                return;

	/* Cleanup Writer's rings - Currently it should be at most 1 */
	psr = pop_ps_ring(&logger->wr);
	while (psr) {
        	push_ps_ring_reset(&logger->chain, psr);
		psr = pop_ps_ring(&logger->wr);
	}

	/* Cleanup Reader's rings */
	psr = pop_ps_ring(&logger->rd);
	while (psr) {
        	push_ps_ring_reset(&logger->chain, psr);
		psr = pop_ps_ring(&logger->rd);
	}

	/* Init  Writer's ring */
	push_ps_ring(&logger->wr, pop_ps_ring(&logger->chain));

        logger->count = 0;
}

void fini_logger(struct data_logger *logger)
{
        /* Enable if individually allocated */
        vfree(logger->ptr);
        vfree(logger);
}

int write_log_sample(struct data_logger *logger, struct data_logger_sample *sample)
{
        struct data_logger_ring *psr;
        
        if (WARN_ONCE(!logger, "WRITE Internal error, logger is NULL\n")) {
                return -1;
        }

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

        memcpy(&psr->buff[psr->idx], sample, sizeof(struct data_logger_sample));

        psr->idx++;
        logger->count++;
        
        if (logger->count % 128 == 0) {
                pr_info("Wrote %u samples on core %u\n",
                        logger->count, logger->cpu);
        }

        return 0;
}

struct data_logger_sample *read_log_sample(struct data_logger *logger)
{
        struct data_logger_sample *sample;
        struct data_logger_ring *psr;
        
        if (WARN_ONCE(!logger, "READ Internal error, logger is NULL\n")) {
                return NULL;
        }

        psr = logger->rd.head;
        
        if (WARN_ONCE(!psr, "Internal error, rd_buff is NULL\n")) {
                return NULL;
        }

        if(psr->idx >= psr->length) {
                push_ps_ring_reset(&logger->chain, pop_ps_ring(&logger->rd));

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

static void flush_written_samples_on_cpu(void *dummy)
{
        unsigned cpu = get_cpu();
        struct data_logger_ring *psr;
        struct data_logger *logger = per_cpu(pcpu_data_logger, cpu);

        pr_debug("Flushing written samples on cpu %u\n", cpu);

        psr = pop_ps_ring(&logger->wr);

	if (!psr)
		goto end;

        if (psr->idx < psr->length) {
                psr->length = psr->idx;
                pr_debug("Cutting ring at %u on cpu %u\n", psr->idx + 1, cpu);
        }

        push_ps_ring(&logger->rd, psr);

        push_ps_ring(&logger->wr, pop_ps_ring(&logger->chain));

end:
        put_cpu();
}

void flush_written_samples_on_system(void)
{
        atomic_inc(&on_samples_flushing);
	
        smp_mb();
        while(atomic_read(&active_pmis))
                ;
        smp_mb();

        on_each_cpu(flush_written_samples_on_cpu, NULL, 1);
        
        atomic_dec(&on_samples_flushing);
}

bool check_log_sample(struct data_logger *logger)
{
        struct data_logger_ring *psr;
        
        if (WARN_ONCE(!logger, "CHECK Internal error, logger is NULL\n")) {
                return NULL;
        }

        psr = logger->rd.head;

        if (!psr)
                return false;

        if (psr->idx >= psr->length) {
                psr = psr->next;
                return psr && psr->idx < psr->length;
        }

        return true;
}
