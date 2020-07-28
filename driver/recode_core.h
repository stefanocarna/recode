#ifndef _RECODE_CORE_H
#define _RECODE_CORE_H

#include <asm/pmc_dynamic.h>

extern int recode_data_init(void);
extern void recode_data_fini(void);

extern int recode_pmc_init(void);
extern void recode_pmc_fini(void);

extern void recode_pmc_test(void);

extern int register_ctx_hook(void);
extern void unregister_ctx_hook(void);

extern int attach_process(pid_t pid);
extern void detach_process(pid_t pid);

struct pmc_logger {
        unsigned cpu;
        unsigned idx;
        unsigned length;
        struct pmc_snapshot buff[];
};

DECLARE_PER_CPU(struct pmc_logger *, pcpu_pmc_logger);

#endif /* _RECODE_CORE_H */