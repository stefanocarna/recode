
#include <asm/msr.h>
#include <linux/sched.h>	
#include <asm/pmc_dynamic.h>
#include <linux/pmc_dynamic.h>

#include "dependencies.h"
#include "device/proc.h"
#include "recode_core.h"

static u64 utime = 0; 
static u64 ktime = 0; 
static u64 otime = 0; 
static bool time_init = false;

static void ctx_hook(struct task_struct *prev, bool prev_on, bool curr_on)
{
	int cpu = get_cpu();

	if (!curr_on)
		goto end;

	if (!time_init) {
		otime = this_cpu_read(pcpu_pmc_snapshot.fixed2);
		time_init = true;
		goto end;
	}

	/* Prev is profiled */
	if (prev_on) {

	} else if (curr_on) {
		/* Current is profiled */
		
	}
	
	utime += current->pmc_user->fixed2; // - this_cpu_read(pcpu_pmc_snapshot.fixed2);
	ktime = this_cpu_read(pcpu_pmc_snapshot.fixed2) - utime - otime;

	pr_info("[%u, %u] utime: %llx, ktime: %llx\n", 
	    cpu, current->pid, utime, ktime);
	
	// pr_info("[%u] FX1: %llx, PMC1: %llx, CORE1: %llx\n",
	//     cpu, current->pmc_user->fixed1, pmc,
	//     this_cpu_read(pcpu_pmc_snapshot.fixed1));
	
end:
	put_cpu();
}

/* Register thread for profiling activity  */
int attach_process(pid_t pid)
{
	pr_info("Attaching pid %u\n", pid);
	return 0;
}

/* Remove registered thread from profiling activity */
void detach_process(pid_t pid)
{

}

int register_ctx_hook(void)
{
	int err = 0;

	hook_register(&ctx_hook);
	// pid_register(15913);
	// switch_hook_resume();
	// test_asm();
	return err;
}

void unregister_ctx_hook(void)
{
	hook_unregister();
}