/* test */

#include "recode_tma.h"

/* L0 */
/*
#define	evt_im_recovery_cycles_bit	BIT_ULL(15)
#define	evt_ui_any_bit	BIT_ULL(14)
#define	evt_iund_core_bit BIT_ULL(16)
#define	evt_ur_retire_slots_bit BIT_ULL(12)
*/

#define evt_im_recovery_cycles_pos (0)
#define evt_ui_any_pos (1)
#define evt_iund_core_pos (2)
#define evt_ur_retire_slots_pos (3)

/* L1 */
/*
#define	evt_ca_stalls_mem_any_bit	BIT_ULL(0)
#define	evt_ea_bound_on_stores_bit	BIT_ULL(10)
*/

#define	evt_ca_stalls_mem_any_pos (4)
#define evt_ea_bound_on_stores_pos (5)

/*
#define	evt_ea_exe_bound_0_ports_bit	BIT_ULL(5)
#define	evt_ea_1_ports_util_bit	BIT_ULL(6)
*/

#define	evt_ea_exe_bound_0_ports_pos (6)
#define evt_ea_1_ports_util_pos (7)

/* L2 */
/*
#define	evt_ca_stalls_l1d_miss_bit	BIT_ULL(4)
#define	evt_ca_stalls_l2_miss_bit	BIT_ULL(3)
#define	evt_ca_stalls_l3_miss_bit	BIT_ULL(2)
#define	evt_l2_hit_bit	BIT_ULL(28)
#define evt_l1_pend_miss_bit	BIT_ULL(29)
*/

#define	evt_ca_stalls_l3_miss_pos	(8)
#define	evt_ca_stalls_l2_miss_pos	(9)
#define	evt_ca_stalls_l1d_miss_pos	(10)
#define	evt_l2_hit_pos	(11)
#define	evt_l1_pend_miss_pos	(12)


/* Frontend Bound */
//#define	L0_FB	(evt_iund_core_bit)
#define	L0_FB	(BIT_ULL(evt_iund_core_pos))
/* Bad Speculation */
//#define	L0_BS	(evt_ur_retire_slots_bit | evt_ui_any_bit | evt_im_recovery_cycles_bit)
#define	L0_BS	(BIT_ULL(evt_ur_retire_slots_pos) | BIT_ULL(evt_ui_any_pos) | BIT_ULL(evt_im_recovery_cycles_pos))
/* Retiring */
//#define L0_RE	(evt_ur_retire_slots_bit)
#define L0_RE	(BIT_ULL(evt_ur_retire_slots_pos))
/* Backend Bound */
#define L0_BB	(L0_FB | L0_BS | L0_RE)

/* Few Uops Executed Threshold */
#define L1_MID_FUET (0)		//???
/* Core Bound Cycles */
//#define L1_MID_CBC	(evt_ea_exe_bound_0_ports_bit | evt_ea_1_ports_util_bit | L1_MID_FUET)
#define L1_MID_CBC	(BIT_ULL(evt_ea_exe_bound_0_ports_pos) | BIT_ULL(evt_ea_1_ports_util_pos) | L1_MID_FUET)
/* Backend Bound Cycles */
//#define L1_MID_BBC (evt_ca_stalls_mem_any_bit | evt_ea_bound_on_stores_bit | L1_MID_CBC)
#define L1_MID_BBC (BIT_ULL(evt_ca_stalls_mem_any_pos) | BIT_ULL(evt_ea_bound_on_stores_pos) | L1_MID_CBC)
/* Memory Bound Fraction */
//#define L1_MID_MBF (evt_ca_stalls_mem_any_bit | evt_ea_bound_on_stores_bit | L1_MID_BBC)
#define L1_MID_MBF (BIT_ULL(evt_ca_stalls_mem_any_pos) | BIT_ULL(evt_ea_bound_on_stores_pos) | L1_MID_BBC)

/* Memory Bound */
#define L1_MB (L0_BB | L1_MID_MBF)
/* Core Bound */
#define L1_CB (L0_BB | L1_MB)

/* L2 Bound Ratio */
//#define	L2_MID_BR	(evt_ca_stalls_l1d_miss_bit | evt_ca_stalls_l2_miss_bit)
#define	L2_MID_BR	(BIT_ULL(evt_ca_stalls_l1d_miss_pos) | BIT_ULL(evt_ca_stalls_l2_miss_pos))

/* L1 Bound */
//#define	L2_L1B	(evt_ca_stalls_mem_any_bit | evt_ca_stalls_l1d_miss_bit)
#define	L2_L1B	(BIT_ULL(evt_ca_stalls_mem_any_pos) | BIT_ULL(evt_ca_stalls_l1d_miss_pos))
/* L3 Bound */
//#define	L2_L3B	(evt_ca_stalls_l2_miss_bit | evt_ca_stalls_l3_miss_bit)
#define	L2_L3B	(BIT_ULL(evt_ca_stalls_l2_miss_pos) | BIT_ULL(evt_ca_stalls_l3_miss_pos))
/* L2 Bound */		/*| evt_ca_stalls_l1d_miss_bit | evt_ca_stalls_l2_miss_bit*/
//#define L2_L2B	(evt_l2_hit_bit | evt_l1_pend_miss_bit | L2_MID_BR)
#define L2_L2B	(BIT_ULL(evt_l2_hit_pos) | BIT_ULL(evt_l1_pend_miss_pos) | L2_MID_BR)
/* DRAM Bound */
//#define	L2_DRAMB	(evt_ca_stalls_l3_miss_bit | L2_MID_BR | L2_L2B)
#define	L2_DRAMB	(BIT_ULL(evt_ca_stalls_l3_miss_pos) | L2_MID_BR | L2_L2B)
/* Store Bound */
//#define	L2_SB	(evt_ea_bound_on_stores_bit)
#define	L2_SB	(BIT_ULL(evt_ea_bound_on_stores_pos))


#define TMA_L0	(L0_BB | L0_BS | L0_FB | L0_RE)
#define	TMA_L1	(L1_CB | L1_MB)
// TMA_L2 + L2_DRAMB + L2_SB
#define	TMA_L2	(L2_L1B | L2_L3B | L2_L2B)

#define computable_tma(tma, mask) (tma & mask) == tma

#define PIPELINE_WIDTH	4
#define SFACT 1000

#define	get_array_element(pmc, event)	(pmc[this_cpu_read(pcpu_pmc_index_array[event])])

#define l0_mid_total_slots(pmc) PIPELINE_WIDTH * pmc[1]

/*
#define	l0_fb_compute(pmc)	((SFACT * pmc[pmc_index_array[16]]) / (l0_mid_total_slots(pmc) + 1))
#define l0_bs_compute(pmc)	((SFACT * (pmc[pmc_index_array[14]] - pmc[pmc_index_array[12]] + (PIPELINE_WIDTH * pmc[pmc_index_array[15]]))) / (l0_mid_total_slots(pmc) + 1))
#define l0_re_compute(pmc) ((SFACT * pmc[pmc_index_array[12]]) / (l0_mid_total_slots(pmc) + 1))
*/
#define	l0_fb_compute(pmc)	((SFACT * get_array_element(pmc, evt_iund_core_pos)) / (l0_mid_total_slots(pmc) + 1))
#define l0_bs_compute(pmc)	((SFACT * (get_array_element(pmc, evt_ui_any_pos) - get_array_element(pmc, evt_ur_retire_slots_pos) + (PIPELINE_WIDTH * get_array_element(pmc, evt_im_recovery_cycles_pos)))) / (l0_mid_total_slots(pmc) + 1))
#define l0_re_compute(pmc) ((SFACT * get_array_element(pmc, evt_ur_retire_slots_pos)) / (l0_mid_total_slots(pmc) + 1))
#define l0_bb_compute(pmc) (SFACT - (l0_fb_compute(pmc) + l0_bs_compute(pmc) + l0_re_compute(pmc)))

/*
#define	l1_mid_cbc_compute(pmc)	(pmc[pmc_index_array[5]] + pmc[pmc_index_array[6]] + L1_MID_FUET)
#define	l1_mid_bbc_compute(pmc)	(l1_mid_cbc_compute(pmc) + pmc[pmc_index_array[0]] + pmc[pmc_index_array[10]])
#define	l1_mid_mbf_compute(pmc)	(SFACT * (pmc[pmc_index_array[0]] + pmc[pmc_index_array[10]])	/	(l1_mid_bbc_compute(pmc) + 1))
*/
#define	l1_mid_cbc_compute(pmc)	(get_array_element(pmc, evt_ea_exe_bound_0_ports_pos) + get_array_element(pmc, evt_ea_1_ports_util_pos) + L1_MID_FUET)
#define	l1_mid_bbc_compute(pmc)	(l1_mid_cbc_compute(pmc) + get_array_element(pmc, evt_ca_stalls_mem_any_pos) + get_array_element(pmc, evt_ea_bound_on_stores_pos))
#define	l1_mid_mbf_compute(pmc)	(SFACT * (get_array_element(pmc, evt_ca_stalls_mem_any_pos) + get_array_element(pmc, evt_ea_bound_on_stores_pos))	/	(l1_mid_bbc_compute(pmc) + 1))

#define	l1_mb_compute(pmc)	((l1_mid_mbf_compute(pmc) * l0_bb_compute(pmc))	/	SFACT)
#define l1_cb_compute(pmc)	(l0_bb_compute(pmc) - l1_mb_compute(pmc))

//#define	l2_mid_br_compute(pmc)	((pmc[pmc_index_array[4]] - pmc[pmc_index_array[3]]) / (pmc[1] + 1))
#define	l2_mid_br_compute(pmc)	(SFACT * (get_array_element(pmc, evt_ca_stalls_l1d_miss_pos) - get_array_element(pmc, evt_ca_stalls_l2_miss_pos)) / (pmc[1] + 1))

/*
#define	l2_l1b_compute(pmc)	((pmc[pmc_index_array[0]] - pmc[pmc_index_array[4]]) / (pmc[1] + 1))
#define	l2_l3b_compute(pmc)	((pmc[pmc_index_array[3]] - pmc[pmc_index_array[2]]) / (pmc[1] + 1))
#define	l2_l2b_compute(pmc)	((pmc[pmc_index_array[39]] / (pmc[pmc_index_array[28]] + pmc[pmc_index_array[29]] + 1)) * l2_mid_br_compute(pmc))
#define	l2_dramb_compute(pmc)	((pmc[pmc_index_array[2]] / (pmc[1] + 1)) + l2_mid_br_compute(pmc) - l2_l2b_compute(pmc))
#define l2_sb_compute(pmc)	(pmc[pmc_index_array[10]] / (pmc[1] + 1))
*/
#define	l2_l1b_compute(pmc)	(SFACT * (get_array_element(pmc, evt_ca_stalls_mem_any_pos) - get_array_element(pmc, evt_ca_stalls_l1d_miss_pos)) / (pmc[1] + 1))
#define	l2_l3b_compute(pmc)	(SFACT * (get_array_element(pmc, evt_ca_stalls_l2_miss_pos) - get_array_element(pmc, evt_ca_stalls_l3_miss_pos)) / (pmc[1] + 1))
#define	l2_l2b_compute(pmc)	(get_array_element(pmc, evt_l2_hit_pos) * l2_mid_br_compute(pmc) / (get_array_element(pmc, evt_l2_hit_pos) + get_array_element(pmc, evt_l1_pend_miss_pos) + 1))

#define	l2_dramb_compute(pmc)	((get_array_element(pmc, evt_ca_stalls_l3_miss_pos) / (pmc[1] + 1)) + l2_mid_br_compute(pmc) - l2_l2b_compute(pmc))
#define l2_sb_compute(pmc)	(get_array_element(pmc, evt_ea_bound_on_stores_pos) / (pmc[1] + 1))

DEFINE_PER_CPU(u8[HW_EVENTS_NUMBER], pcpu_pmc_index_array);
DEFINE_PER_CPU(u8, pcpu_current_tma_lvl) = 0;

//u64 pmc_index_array[50];


void check_tma(u32 metrics_size, u64 metrics[], u64 mask) {
	u32 idx;

	for(idx=0; idx<metrics_size; ++idx)
		pr_debug("metric %u check result: %s\n", idx, computable_tma(metrics[idx], mask) ? "ok" : "missing event");
}


void print_pmcs_collection(struct pmcs_collection *collection){
	u32 idx;

	for(idx=0; idx<collection->cnt; ++idx)
		pr_debug("%llx ", collection->pmcs[idx]);
}

/*
//	O(n!) if used for whole array
u64 compute_pmc_index(u32 idx, u64 mask){
	u32 tmp_idx;
	u64 pmc_idx=0;
	for(tmp_idx=0; tmp_idx<idx; ++tmp_idx){
		if(mask & BIT_ULL(tmp_idx))
			pmc_idx++;
	}
	return pmc_idx + 3;
}
*/

//	O(n)
void update_index_array(struct hw_events *events){
	u8 idx;
	u8 tmp=0;
	pr_debug("updating index_array on cpu %u\n", smp_processor_id());
	for(idx=0; tmp<events->cnt; ++idx){
		if(events->mask & BIT_ULL(idx)){
			pr_debug("found event on position: %u, val: %u\n", idx, tmp + 3);
			this_cpu_write(pcpu_pmc_index_array[idx], tmp + 3);
			tmp++;
		}
	}
}


void compute_tma(struct pmcs_collection *collection, u64 mask, u8 cpu){

/*
	if (computable_tma(TMA_L0, mask)){
		pr_debug("L0_FB: %llu\n", l0_fb_compute(collection->pmcs));
		pr_debug("L0_BS: %llu\n", l0_bs_compute(collection->pmcs));
		pr_debug("L0_RE: %llu\n", l0_re_compute(collection->pmcs));
		pr_debug("L0_BB: %llu\n", l0_bb_compute(collection->pmcs));
	}
	if (computable_tma(TMA_L1, mask)){
		pr_debug("L1_MB: %llu\n", l1_mb_compute(collection->pmcs));
		pr_debug("L1_CB: %llu\n", l1_cb_compute(collection->pmcs));
	}
	if (computable_tma(TMA_L2, mask)){
		pr_debug("L2_L1B: %llu\n", l2_l1b_compute(collection->pmcs));
		pr_debug("L2_L2B: %llu\n", l2_l2b_compute(collection->pmcs));
		pr_debug("L2_L3B: %llu\n", l2_l3b_compute(collection->pmcs));
		pr_debug("L2_DRAMB: %llu\n", l2_dramb_compute(collection->pmcs));
		pr_debug("L2_SB: %llu\n", l2_sb_compute(collection->pmcs));
	}*/

	/*
	for(i=0; i<gbl_nr_hw_events; ++i){
		if (gbl_hw_events[i].mask == mask)
			setup_hw_events_on_cpu(gbl_hw_events[i]);
	}*/
/*
	for(i=0; i<gbl_nr_hw_events; ++i){
		setup_hw_events_on_cpu(gbl_hw_events[i]);
		pr_debug("global %u: %llu", i, this_cpu_read(pcpu_pmus_metadata.hw_events)->mask);
		if(computable_tma(TMA_L0, this_cpu_read(pcpu_pmus_metadata.hw_events)->mask))
			pr_debug("tma 0, cycle %u\n", i);
		if(computable_tma(TMA_L1, this_cpu_read(pcpu_pmus_metadata.hw_events)->mask))
			pr_debug("tma 1, cycle %u\n", i);
		if(computable_tma(TMA_L2, this_cpu_read(pcpu_pmus_metadata.hw_events)->mask))
			pr_debug("tma 2, cycle %u\n", i);
	}
	*/
/*
	switch (this_cpu_read(pcpu_current_tma_lvl)) {

		case 0:
			if (computable_tma(TMA_L0), mask){
				pr_debug("L0_FB: %llu\n", l0_fb_compute(collection->pmcs));
				pr_debug("L0_BS: %llu\n", l0_bs_compute(collection->pmcs));
				pr_debug("L0_RE: %llu\n", l0_re_compute(collection->pmcs));
				pr_debug("L0_BB: %llu\n", l0_bb_compute(collection->pmcs));
				if (l0_bb_compute(collection->pmcs) > TMA_L0_THRESHOLD){
					setup_hw_events_on_cpu(gbl_hw_events[1]);
					this_cpu_write(pcpu_current_tma_lvl, 1);
				}
			}
			else{
				pr_debug("Can't compute current TMA_L0 metrics\n");
			}
			break;

		case 1:
			if (computable_tma(TMA_L1), mask){
				pr_debug("L1_MB: %llu\n", l1_mb_compute(collection->pmcs));
				pr_debug("L1_CB: %llu\n", l1_cb_compute(collection->pmcs));
				if (l1_mb_compute(collection->pmcs) > TMA_L1_THRESHOLD){
					setup_hw_events_on_cpu(gbl_hw_events[2]);
					this_cpu_write(pcpu_current_tma_lvl, 2);
				} else {
					setup_hw_events_on_cpu(gbl_hw_events[0]);
					this_cpu_write(pcpu_current_tma_lvl, 0);
				}
			}
			else{
				pr_debug("Can't compute current TMA_L1 metrics\n");
			}
			break;

		case 2:
			if (computable_tma(TMA_L2), mask){
				pr_debug("L2_L1B: %llu\n", l2_l1b_compute(collection->pmcs));
				pr_debug("L2_L2B: %llu\n", l2_l2b_compute(collection->pmcs));
				pr_debug("L2_L3B: %llu\n", l2_l3b_compute(collection->pmcs));
			}
			else{
				pr_debug("Can't compute current TMA_L2 metrics\n");
			}
			setup_hw_events_on_cpu(gbl_hw_events[1]);
			this_cpu_write(pcpu_current_tma_lvl, 1);
			break;
	}
	*/

	u8 curr=this_cpu_read(pcpu_current_tma_lvl);
	if (curr==0){
		if (computable_tma(TMA_L0, mask)){
			pr_debug("L0_FB: %llu\n", l0_fb_compute(collection->pmcs));
			pr_debug("L0_BS: %llu\n", l0_bs_compute(collection->pmcs));
			pr_debug("L0_RE: %llu\n", l0_re_compute(collection->pmcs));
			pr_debug("L0_BB: %llu\n", l0_bb_compute(collection->pmcs));
			setup_hw_events_on_cpu(gbl_hw_events[1]);
			this_cpu_write(pcpu_current_tma_lvl, 1);
		}
	}
	if (curr==1){
		if (computable_tma(TMA_L1, mask)){
			pr_debug("CBC: %llx\n", l1_mid_cbc_compute(collection->pmcs));
			pr_debug("BBC: %llx\n", l1_mid_bbc_compute(collection->pmcs));
			pr_debug("MBF: %llx\n", l1_mid_mbf_compute(collection->pmcs));
			pr_debug("L1_MB: %llx\n", l1_mb_compute(collection->pmcs));
			pr_debug("L1_CB: %llx\n", l1_cb_compute(collection->pmcs));
			setup_hw_events_on_cpu(gbl_hw_events[2]);
			this_cpu_write(pcpu_current_tma_lvl, 2);
		}
	}
	if (curr==2){
		if (computable_tma(TMA_L2, mask)){
			pr_debug("stalls_mem_any %llx\n", get_array_element(collection->pmcs, evt_ca_stalls_mem_any_pos));
			pr_debug("stalls_l1d_miss %llx\n", get_array_element(collection->pmcs, evt_ca_stalls_l1d_miss_pos));
			pr_debug("L2_L1B: %llu\n", l2_l1b_compute(collection->pmcs));
			pr_debug("L2_L2B: %llu\n", l2_l2b_compute(collection->pmcs));
			pr_debug("L2_L3B: %llu\n", l2_l3b_compute(collection->pmcs));
		}
	}
}


/******************/
