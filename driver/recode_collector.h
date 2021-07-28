#ifndef _RECODE_COLLECTOR_H
#define _RECODE_COLLECTOR_H

#include "pmu/pmu.h"
#include "recode_config.h"

DECLARE_PER_CPU(struct data_logger *, pcpu_data_logger);

extern atomic_t on_samples_flushing;

struct data_logger_sample {
	pid_t id;
	unsigned tracked;
	unsigned k_thread;
	unsigned ctx_evt;
	struct pmcs_snapshot pmcs;
};

struct data_logger_ring {
	unsigned idx;
	unsigned length;
	struct data_logger_ring *next;
	struct data_logger_sample buff[RING_BUFF_LENGTH];
};

struct data_logger_chain {
	spinlock_t lock;
	struct data_logger_ring *head;
	struct data_logger_ring *tail;
};

struct data_logger {
	unsigned cpu;
	unsigned count;
	struct data_logger_ring *ptr;
	struct data_logger_chain chain;
	struct data_logger_chain wr;
	struct data_logger_chain rd;
};

extern struct data_logger *init_logger(unsigned cpu);
extern void fini_logger(struct data_logger *logger);
extern void reset_logger(struct data_logger *logger);

extern int write_log_sample(struct data_logger *logger,
                            struct data_logger_sample *sample);

extern struct data_logger_sample *read_log_sample(struct data_logger *logger);

extern bool check_log_sample(struct data_logger *logger);

extern int flush_logs(struct data_logger *logger);

extern bool push_ps_ring(struct data_logger_chain *chain,
                             struct data_logger_ring *ring);

extern bool push_ps_ring_reset(struct data_logger_chain *chain,
                             struct data_logger_ring *ring);

extern void flush_written_samples_on_system(void);

extern struct data_logger_ring *pop_ps_ring(struct data_logger_chain *chain);

#endif /* _RECODE_COLLECTOR_H */