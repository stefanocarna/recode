#include "recode.h"
#include "recode_pmu.h"

#include "pmc_events.h"

static const typeof(evt_null) evt_set[] = {
	evt_ur_retire_slots,
	evt_ui_any,
	evt_im_recovery_cycles,
	evt_iund_core,

	evt_ca_stalls_mem_any,
	evt_ea_bound_on_stores,
	evt_ea_exe_bound_0_ports,
	evt_ea_1_ports_util
};

// TMA Level 0
#define	evt_ur_retire_slots_shf		BIT_ULL(0) // 0
#define	evt_ui_any_shf			BIT_ULL(1) // 1
#define	evt_im_recovery_cycles_shf	BIT_ULL(2) // 2
#define	evt_iund_core_shf		BIT_ULL(3) // 3
	
// #Memory_Bound_Fraction
#define	evt_ca_stalls_mem_any_shf	BIT_ULL(4) // 4
#define	evt_ea_bound_on_stores_shf	BIT_ULL(5) // 5

// #Core_Bound_Cycles
#define	evt_ea_exe_bound_0_ports_shf	BIT_ULL(6) // 6
#define	evt_ea_1_ports_util_shf 	BIT_ULL(7) // 7


/* Frontend Bound */
#define L0_FB 		(evt_iund_core_shf)
/* Bad Speculation */
#define L0_BS 		(evt_ur_retire_slots_shf | evt_ui_any_shf | \
                         evt_im_recovery_cycles_shf)
/* Retiring */
#define L0_RE 		(evt_im_recovery_cycles_shf)
/* Backend Bound */
#define L0_BB 		(L0_FB | L0_BS | L0_RE)

/* Few Uops Executed Threshold */
#define L1_FUET		(0)
/* Core Bound Cycles */
#define L1_CBC		(evt_ea_exe_bound_0_ports_shf | \
                         evt_ea_1_ports_util_shf | L1_FUET)
/* Backend Bound Cycles */
#define L1_BBC		(evt_ca_stalls_mem_any_shf | \
			 evt_ea_bound_on_stores_shf | L1_CBC)
/* Memory Bound Fraction */
#define L1_MBF		(evt_ca_stalls_mem_any_shf | \
			 evt_ea_bound_on_stores_shf | L1_BBC)
/* Memory Bound*/
#define L1_MB		(L0_BB | L1_MBF)
/* Core Bound */
#define L1_CB		(L0_BB | L1_MB)


#define TMA_L0		(L0_BB | L0_FB | L0_BS | L0_RE)
#define TMA_L1		(L1_MB | L1_CB)

static const u64 tma_selectable_level[] = {
	TMA_L0,
	TMA_L1
};

#define SFACT		1000
#define PIPELINE_WIDTH	4

#define cmp_slots(x)	(PIPELINE_WIDTH * x->fixed[1])
#define	cmp_l0_fb(x)	((SFACT * x->general[3]) /(cmp_slots(x) + 1))
#define	cmp_l0_re(x)	((SFACT * x->general[0]) /(cmp_slots(x) + 1))
#define	cmp_l0_bs(x)	((SFACT * (x->general[1] - x->general[0] + \
                         (PIPELINE_WIDTH * x->general[2]))) / (cmp_slots(x) + 1))
#define	cmp_l0_bb(x)	(SFACT - cmp_l0_fb(x) - cmp_l0_re(x) - cmp_l0_bs(x))

#define cmp_l1_fuet(x)	(0)
#define cmp_l1_cbc(x)	((x->general[6] + x->general[7]) + cmp_l1_fuet(x))
#define cmp_l1_bbc(x)	((x->general[4] + x->general[5]) + cmp_l1_cbc(x))
#define cmp_l1_mbf(x)	(((x->general[4] + x->general[5]) * SFACT) / \
                         (cmp_l1_bbc(x) + 1))
#define cmp_l1_mb(x)	((cmp_l1_mbf(x) * cmp_l0_bb(x)) / SFACT)
#define cmp_l1_cb(x)	((cmp_l0_bb(x)) - cmp_l1_mb(x))


#define DEFINE_CMD_WRP(x) u64 __##x (struct pmcs_snapshot *pmcs) \
			{ return x(pmcs); }

static __always_inline DEFINE_CMD_WRP(cmp_l0_fb)
static __always_inline DEFINE_CMD_WRP(cmp_l0_re)
static __always_inline DEFINE_CMD_WRP(cmp_l0_bs)
static __always_inline DEFINE_CMD_WRP(cmp_l0_bb)
static __always_inline DEFINE_CMD_WRP(cmp_l1_mb)
static __always_inline DEFINE_CMD_WRP(cmp_l1_cb)

static __always_inline DEFINE_CMD_WRP(cmp_l1_cbc)


struct tma_object {
	char name[255];
	u64 mask;
	// unsigned cmp_idx;
	u64 (*cmp)(struct pmcs_snapshot *pmcs);
};

static const struct tma_object tma_builtin_level[] = {
	{"FRONTEND", L0_FB, __cmp_l0_fb},
	{"RETIRING", L0_RE, __cmp_l0_re},
	{"SPECULATION", L0_BS, __cmp_l0_bs},
	{"BACKEND", L0_BB, __cmp_l0_bb},

	{"CBC", L1_CBC, __cmp_l1_cbc},

	{"MEMORY", L1_MB, __cmp_l1_mb},
	{"CORE", L1_CB, __cmp_l1_cb},
};

static u64 active_evts = 0ULL;

#define TEST_PROGRAM "stress-ng"

static pmc_evt_code tma_pmc_events[8];

void change_tma_level(unsigned level) 
{
	unsigned i, k;
	// TODO DATA LEAK PROBLEM
	// pmc_evt_code pmc_events[8];
	tma_pmc_events[0] = 0;
	tma_pmc_events[1] = 0;
	tma_pmc_events[2] = 0;
	tma_pmc_events[3] = 0;
	tma_pmc_events[4] = 0;
	tma_pmc_events[5] = 0;
	tma_pmc_events[6] = 0;
	tma_pmc_events[7] = 0;

	recode_set_state(OFF);

	/* Change level */
	active_evts = tma_selectable_level[level];

	for (i = 0, k = 0; k < sizeof(u64) && i < 8; ++k) {
		if (active_evts & BIT_ULL(k)) {
			tma_pmc_events[i] = evt_set[k];
			++i;
		}
	}

	setup_pmc_on_system(tma_pmc_events);

	recode_set_state(PROFILE);
}

unsigned done = 0;

unsigned p1 = 5;


void pmc_evaluate_tma(unsigned cpu, struct pmcs_snapshot *pmcs)
{
	unsigned i;

	for (i = 0; TEST_PROGRAM[i]; ++i)
		if (current->comm[i] != TEST_PROGRAM[i])
			return;

	if (!done) {
		change_tma_level(0);
		done = 1;
	}

	if (p1 && p1--) {
		pr_info("*** [%u] MTA analysis: %llx\n", current->pid, active_evts);
		for (i = 0; i < sizeof(tma_builtin_level) / sizeof(struct tma_object); ++i)
			if (tma_builtin_level[i].mask & active_evts)
				pr_info("* %s:  %llu\n", tma_builtin_level[i].name,
					tma_builtin_level[i].cmp(pmcs));
		pr_info("***\n\n");
	}

	if (done < 2 && cmp_l0_bb(pmcs) > (300)) {
		// TODO Restore
		// change_tma_level(1);
		done = 2;
	}
	
	
	// pr_info("* FRONTEND      %llu\n", CMP_L0_FB(pmcs));
	// pr_info("* BACKEND       %llu\n", CMP_L0_BB(pmcs));
	// pr_info("* RETIRING      %llu\n", CMP_L0_RE(pmcs));
	// pr_info("* SPECULATION   %llu\n", CMP_L0_BS(pmcs));

	write_log_sample(per_cpu(pcpu_pmc_logger, cpu), pmcs);
}