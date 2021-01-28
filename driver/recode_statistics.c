#include "recode.h"

DEFINE_PER_CPU(struct statistic, pcpu_statistics) = { 0 };

struct pmc_logger *init_logger(unsigned cpu)
{
        struct pmc_logger *logger;
        logger = vmalloc(sizeof(struct pmc_logger) + (BUFF_LENGTH * sizeof(struct pmcs_snapshot)));
	if (!logger) 
		return NULL;
	reset_logger(logger);
	logger->cpu = cpu;

        return logger;
}

void reset_logger(struct pmc_logger *logger)
{
        if (!logger)
                return;

        logger->idx = 0;
	logger->length = BUFF_LENGTH;
}

void fini_logger(struct pmc_logger *logger)
{
        vfree(logger);
}

int log_sample(struct pmc_logger *logger, struct pmcs_snapshot *sample)
{
        if (!logger) {
                pr_info("Internal error, logger is NULL\n");
                return -1;
        }
        
        if(logger->idx >= logger->length) {
                pr_info("Cannot log on cpu %u, logger is FULL\n", logger->cpu);
                return -1;
        }

        if (logger->idx % 128 == 0) {
                pr_info("Registered %u samples on core %u\n",
                        logger->idx, logger->cpu);
        }

        memcpy(&logger->buff[logger->idx], sample, sizeof(typeof(*sample)));

        logger->idx++;
        return 0;
}

int flush_logs(struct pmc_logger *logger)
{
        /* TODO IMPLEMENT */
        return 0;
}

void process_match(struct task_struct *tsk)
{

}
