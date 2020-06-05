#ifndef _RECODE_CORE_H
#define _RECODE_CORE_H

extern int register_ctx_hook(void);
extern void unregister_ctx_hook(void);

extern int attach_process(pid_t pid);
extern void detach_process(pid_t pid);

#endif /* _RECODE_CORE_H */