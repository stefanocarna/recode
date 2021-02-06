
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

struct pmc_logger {
	unsigned cpu;
	unsigned idx;
	unsigned length;
	struct pmcs_snapshot buff[];
};

struct statistic {
	u64 tsc;
	unsigned cpu;
	char *name;
	// u64 last_sample;
	struct statistic *next;
};

struct detect_stats {
	/* Voluntary CTX before detection */
	unsigned nvcsw;	
	/* UnVoluntary CTX before detection */
	unsigned nivcsw;
	/* Execution time */
	u64 utime;
	u64 stime;
	/* Process data*/
	pid_t pid;
	pid_t tgid;
	char comm[32];
	/* PMIs since first detection */
	unsigned pmis;
	unsigned skpmis;
};