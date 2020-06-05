#ifndef _DEPENDENCIES_H
#define _DEPENDENCIES_H

extern int idt_patcher_install_entry(unsigned long handler, unsigned vector, unsigned dpl);

typedef void ctx_func(struct task_struct *prev, bool prev_on, bool curr_on);

extern void switch_hook_pause(void);

extern void switch_hook_resume(void);

extern void switch_hook_set_mode(unsigned mode);

extern int hook_register(ctx_func *hook);

extern void hook_unregister(void);

extern int pid_register(pid_t pid);

extern int pid_unregister(pid_t pid);

#endif /* _DEPENDENCIES_H */