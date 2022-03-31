#include <linux/kernel_stat.h>

#include "system/stats.h"

void read_cpu_stats(u64 *used, u64 *total)
{
	int i;
	u64 _used = 0, _unused = 0;

	for_each_online_cpu(i) {
		struct kernel_cpustat kcpustat;
		u64 *cpustat = kcpustat.cpustat;

		kcpustat_cpu_fetch(&kcpustat, i);

		_used		+= cpustat[CPUTIME_USER];
		_used		+= cpustat[CPUTIME_NICE];
		_used		+= cpustat[CPUTIME_SYSTEM];

		_unused		+= cpustat[CPUTIME_IDLE];
		_unused		+= cpustat[CPUTIME_IOWAIT];
		_unused		+= cpustat[CPUTIME_IRQ];
		_unused		+= cpustat[CPUTIME_SOFTIRQ];
		_unused		+= cpustat[CPUTIME_STEAL];
		_unused		+= cpustat[CPUTIME_GUEST];
		_unused		+= cpustat[CPUTIME_GUEST_NICE];
	}

	*used = _used;
	*total = _used + _unused;
}
