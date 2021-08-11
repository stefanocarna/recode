/* test */

#include "recode_collector.h"

#define	evt_iund_core_bit BIT_ULL(16)
#define	evt_ur_retire_slots_bit BIT_ULL(12)
#define	evt_ui_any_bit	BIT_ULL(14)
#define	evt_im_recovery_cycles_bit	BIT_ULL(15)

#define	L0_FB_TEST	(evt_iund_core_bit)
#define	L0_BS_TEST	(evt_ur_retire_slots_bit | evt_ui_any_bit | evt_im_recovery_cycles_bit)
#define L0_RE_TEST	(evt_ur_retire_slots_bit)
#define L0_BB_TEST	(L0_FB_TEST | L0_BS_TEST | L0_RE_TEST)

#define COMPUTABLE_TMA(tma, mask) (tma & mask) == tma

#define PIPELINE_WIDTH	4
#define SFACT 1000

#define total_slots(pmc) PIPELINE_WIDTH * pmc[1]

#define	L0_FB_COMPUTE_TEST(pmc)	(SFACT * pmc[pmc_index_array[16]]) / (total_slots(pmc) + 1)
#define L0_BS_COMPUTE_TEST(pmc)	(SFACT * (pmc[pmc_index_array[14]] - pmc[pmc_index_array[12]] + (PIPELINE_WIDTH * pmc[pmc_index_array[15]]))) / (total_slots(pmc) + 1)
#define L0_RE_COMPUTE_TEST(pmc) (SFACT * pmc[pmc_index_array[12]]) / (total_slots(pmc) + 1)
#define L0_BB_COMPUTE_TEST(pmc) SFACT - (L0_FB_COMPUTE_TEST(pmc) + L0_BS_COMPUTE_TEST(pmc) + L0_RE_COMPUTE_TEST(pmc))

/*
struct tma_object {
	char name[255];
	u64 mask;
	u64 (*cmp)(struct pmcs_collection *pmcs);
};

#define DEFINE_CMD_WRP(x)
	u64 __##x(struct pmcs_collection *pmcs)
	{
		return x(pmcs);
	}

static __always_inline DEFINE_CMD_WRP(L0_FB_COMPUTE_TEST)
static __always_inline DEFINE_CMD_WRP(L0_BS_COMPUTE_TEST)
static __always_inline DEFINE_CMD_WRP(L0_RE_COMPUTE_TEST)
static __always_inline DEFINE_CMD_WRP(L0_BB_COMPUTE_TEST)
static __always_inline DEFINE_CMD_WRP(cmp_l1_mb)
static __always_inline DEFINE_CMD_WRP(cmp_l1_cb)
static __always_inline DEFINE_CMD_WRP(cmp_l1_cbc)

static const struct tma_object tma_builtin_level[] = {
	{ "FRONTEND", L0_FB, __L0_FB_COMPUTE_TEST },
	{ "SPECULATION", L0_BS, __L0_BS_COMPUTE_TEST },
	{ "RETIRING", L0_RE, __L0_RE_COMPUTE_TEST },
	{ "BACKEND", L0_BB, __L0_BB_COMPUTE_TEST },

	{ "CBC", L1_CBC, __cmp_l1_cbc },

	{ "MEMORY", L1_MB, __cmp_l1_mb },
	{ "CORE", L1_CB, __cmp_l1_cb },
};
*/

void check_tma(u32 metrics_size, u64 metrics[], u64 mask) {
	u32 idx;

	for(idx=0; idx<metrics_size; ++idx)
		pr_debug("metric %u check result: %s\n", idx, COMPUTABLE_TMA(metrics[idx], mask) ? "ok" : "missing event");
}


void print_pmcs_collection(struct pmcs_collection *collection){
	u32 idx;

	for(idx=0; idx<collection->cnt; ++idx)
		pr_debug("%llx ", collection->pmcs[idx]);
}


u64 compute_pmc_index(u32 idx, u64 mask){
	u32 tmp_idx;
	u64 pmc_idx=0;

	for(tmp_idx=0; tmp_idx<idx; ++tmp_idx){
		if(mask & BIT_ULL(tmp_idx))
			pmc_idx++;
	}

	return pmc_idx + 3;
}


void compute_tma(struct pmcs_collection *collection, u64 mask){
	u64 pmc_index_array[50];
/*
	if (COMPUTABLE_TMA(L0_FB_TEST, mask)){
		pmc_index_array[16] = compute_pmc_index(16, mask);
		//pr_debug("idx16: %llx, slots: %llu\n", collection->pmcs[pmc_index_array[16]], total_slots(collection->pmcs));
		pr_debug("L0_FB_TEST: %llu\n", L0_FB_COMPUTE_TEST(collection->pmcs));
	}
	if (COMPUTABLE_TMA(L0_BS_TEST, mask)){
		pmc_index_array[12] = compute_pmc_index(12, mask);
		pmc_index_array[14] = compute_pmc_index(14, mask);
		pmc_index_array[15] = compute_pmc_index(15, mask);
		//pr_debug("idx12: %llx, idx14: %llx, idx15: %llx\n", collection->pmcs[pmc_index_array[12]], collection->pmcs[pmc_index_array[14]], collection->pmcs[pmc_index_array[15]]);
		pr_debug("L0_BS_TEST: %llu\n", L0_BS_COMPUTE_TEST(collection->pmcs));
	}*/
	if (COMPUTABLE_TMA(L0_RE_TEST, mask)){
		pmc_index_array[12] = compute_pmc_index(12, mask);
		//pr_debug("idx12: %llx\n", collection->pmcs[pmc_index_array[16]]);
		pr_debug("L0_RE_TEST: %llu\n", L0_RE_COMPUTE_TEST(collection->pmcs));
	}/*
	if (COMPUTABLE_TMA(L0_BB_TEST, mask)){
		pmc_index_array[12] = compute_pmc_index(12, mask);
		pmc_index_array[14] = compute_pmc_index(14, mask);
		pmc_index_array[15] = compute_pmc_index(15, mask);
		pmc_index_array[16] = compute_pmc_index(16, mask);
		//pr_debug("idx12: %llx, idx14: %llx, idx15: %llx, idx16: %llx\n", collection->pmcs[pmc_index_array[12]], collection->pmcs[pmc_index_array[14]], collection->pmcs[pmc_index_array[15]], collection->pmcs[pmc_index_array[16]]);
		pr_debug("L0_BB_TEST: %llu\n", L0_BB_COMPUTE_TEST(collection->pmcs));
	}*/
}


/******************/
