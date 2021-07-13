#ifndef _RECODE_CORE_H
#define _RECODE_CORE_H

#include <asm/bug.h>
#include <asm/msr.h>
#include <linux/sched.h>
#include <linux/string.h>

#if __has_include(<asm/fast_irq.h>)
#define FAST_IRQ_ENABLED 1
#endif

#define MODNAME	"ReCode"

#undef pr_fmt
#define pr_fmt(fmt) MODNAME ": " fmt


#define RING_BUFF_LENGTH 64
#define RING_SIZE (sizeof(struct pmcs_snapshot_ring))

#define RING_COUNT 128
#define BUFF_MEMORY RING_COUNT * RING_SIZE

enum recode_state {
	OFF = 0,
	TUNING = 1,
	PROFILE = 2,
	SYSTEM = 3,
	IDLE = 4, // Useless
};

extern enum recode_state __read_mostly recode_state;

/* Recode structs */
struct pmc_evt_sel {
	union {
		u64 perf_evt_sel;
		struct {
			u64 evt: 8, umask: 8, usr: 1, os: 1, edge: 1, pc: 1, pmi: 1,
			any: 1, en: 1, inv: 1, cmask: 8, reserved: 32;
		};
	};
} __attribute__((packed));

typedef u64 pmc_ctr;

struct pmcs_snapshot {
	u64 tsc;
	union {
		pmc_ctr pmcs[11];
		struct {
			pmc_ctr fixed[3];
			pmc_ctr general[8];
		};
	};
};

struct pmcs_snapshot_ring {
	unsigned idx;
	unsigned length;
	struct pmcs_snapshot_ring *next;
	struct pmcs_snapshot buff[RING_BUFF_LENGTH];
};

struct pmcs_snapshot_chain {
	spinlock_t lock;
	struct pmcs_snapshot_ring *head;
	struct pmcs_snapshot_ring *tail;
};

struct pmc_logger {
	unsigned cpu;
	unsigned count;
	struct pmcs_snapshot_ring *ptr;
	struct pmcs_snapshot_chain chain;
	struct pmcs_snapshot_chain wr;
	struct pmcs_snapshot_chain rd;
};

struct statistic {
	u64 tsc;
	unsigned cpu;
	char *name;
	// u64 last_sample;
	struct statistic *next;
};

/* Recode module */
#define for_each_pmc(pmc, max) for ((pmc) = 0; (pmc) < (max); ++(pmc))

#define for_each_fixed_pmc(pmc) for_each_pmc(pmc, max_pmc_fixed)
#define for_each_general_pmc(pmc) for_each_pmc(pmc, max_pmc_general)

extern int recode_data_init(void);
extern void recode_data_fini(void);

extern int recode_pmc_init(void);
extern void recode_pmc_fini(void);

extern void recode_set_state(unsigned state);

extern int register_ctx_hook(void);
extern void unregister_ctx_hook(void);

extern int attach_process(pid_t id);
extern void detach_process(pid_t id);

/* Statistic Unit */
extern struct pmc_logger *init_logger(unsigned cpu);
extern void fini_logger(struct pmc_logger *logger);
extern void reset_logger(struct pmc_logger *logger);

extern int write_log_sample(struct pmc_logger *logger,
                            struct pmcs_snapshot *sample);

extern struct pmcs_snapshot *read_log_sample(struct pmc_logger *logger);
                            ;
extern int flush_logs(struct pmc_logger *logger);

extern bool push_ps_ring(struct pmcs_snapshot_chain *chain,
                             struct pmcs_snapshot_ring *ring);

extern struct pmcs_snapshot_ring *pop_ps_ring(struct pmcs_snapshot_chain *chain);

DECLARE_PER_CPU(struct pmc_logger *, pcpu_pmc_logger);

DECLARE_PER_CPU(bool, pcpu_pmcs_active);

DECLARE_PER_CPU(unsigned long, pcpu_pmi_counter);
/* TODO Enable in the future */
// DECLARE_PER_CPU(u64, pcpu_reset_period);

extern void pmc_evaluate_activity(struct task_struct *tsk, bool log,
				  bool pmc_off);

/* Recode Config */
extern u64 reset_period;

/* Recode PMI */
extern void pmi_function(unsigned cpu);

/* Recode TMA */
extern void pmc_evaluate_tma(unsigned cpu, struct pmcs_snapshot *pmcs);

#endif /* _RECODE_CORE_H */