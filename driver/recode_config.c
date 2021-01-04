#include "recode.h"

// NOTE it was "c << 24". This has been changed because of compatibility
#define evt_umask_cmask(e, u, c)	(0U | e | (u << 8) | (c << 16))

#define evt_ca_stalls_mem_any		evt_umask_cmask(0xa3, 0x10, 16)
#define evt_ca_stalls_total		evt_umask_cmask(0xa3, 0x04, 4)
#define evt_ca_stalls_l3_miss		evt_umask_cmask(0xa3, 0x06, 6)
#define evt_ca_stalls_l2_miss		evt_umask_cmask(0xa3, 0x05, 5)
#define evt_ca_stalls_l1d_miss		evt_umask_cmask(0xa3, 0x0c, 12)
#define evt_ea_bound_on_stores		evt_umask_cmask(0xa6, 0x40, 0)

#define evt_ur_retire_slots		evt_umask_cmask(0xc2, 0x02, 0)
#define evt_iund_core			evt_umask_cmask(0x9c, 0x01, 0)
#define evt_ui_any			evt_umask_cmask(0x0e, 0x01, 0)
#define evt_im_recovery_cycles_any	evt_umask_cmask(0x0d, 0x01, 0)

// TLB
#define stlb_miss_loads			evt_umask_cmask(0xd0, 0x11, 0)
#define tlb_page_walk			evt_umask_cmask(0x08, 0x01, 0)

// Level 2
#define evt_loads_all			evt_umask_cmask(0xd0, 0x81, 0)
#define evt_stores_all			evt_umask_cmask(0xd0, 0x82, 0)

// Not used
#define evt_l1_hit			evt_umask_cmask(0xd1, 0x01, 0)
#define evt_l1_miss			evt_umask_cmask(0xd1, 0x08, 0)

#define evt_l2_hit			evt_umask_cmask(0xd1, 0x02, 0)
#define evt_l2_miss			evt_umask_cmask(0xd1, 0x10, 0)

#define evt_l3_hit			evt_umask_cmask(0xd1, 0x04, 0)
#define evt_l3_miss			evt_umask_cmask(0xd1, 0x20, 0)

#define evt_l3h_xsnp_miss		evt_umask_cmask(0xd2, 0x01, 0)
#define evt_l3h_xsnp_hit		evt_umask_cmask(0xd2, 0x02, 0)
#define evt_l3h_xsnp_hitm		evt_umask_cmask(0xd2, 0x04, 0)
#define evt_l3h_xsnp_none		evt_umask_cmask(0xd2, 0x08, 0)

#define evt_llc_reference		evt_umask_cmask(0x2e, 0x4f, 0)
#define evt_llc_misses			evt_umask_cmask(0x2e, 0x41, 0)

#define evt_l3_miss_data		evt_umask_cmask(0xb0, 0x10, 0)

/* L2 Events */

#define evt_l2_reference		evt_umask_cmask(0x24, 0xef, 0)	// Undercounts
#define evt_l2_misses			evt_umask_cmask(0x24, 0x3f, 0)	

#define evt_l2_all_rfo			evt_umask_cmask(0x24, 0xe2, 0)
#define evt_l2_rfo_misses		evt_umask_cmask(0x24, 0x22, 0)

#define evt_l2_all_data			evt_umask_cmask(0x24, 0xe1, 0)
#define evt_l2_data_misses		evt_umask_cmask(0x24, 0x21, 0)

#define evt_l2_all_code			evt_umask_cmask(0x24, 0xe4, 0)
#define evt_l2_code_misses		evt_umask_cmask(0x24, 0x24, 0)

#define evt_l2_all_pre			evt_umask_cmask(0x24, 0xf8, 0)
#define evt_l2_pre_misses		evt_umask_cmask(0x24, 0x38, 0)

#define evt_l2_all_demand		evt_umask_cmask(0x24, 0xe7, 0)
#define evt_l2_demand_misses		evt_umask_cmask(0x24, 0x27, 0)

#define evt_l2_wb			evt_umask_cmask(0xf0, 0x40, 0)
#define evt_l2_in_all			evt_umask_cmask(0xf1, 0x07, 0)

#define evt_l2_out_silent		evt_umask_cmask(0xf2, 0x01, 0)
#define evt_l2_out_non_silent		evt_umask_cmask(0xf2, 0x02, 0)
#define evt_l2_out_useless		evt_umask_cmask(0xf2, 0x04, 0)


pmc_evt_code pmc_events[8] = {
	evt_l2_all_data,
	evt_l2_data_misses,
	evt_l3_miss_data,
	tlb_page_walk,
	evt_l2_in_all,
	evt_l2_wb,
	tlb_page_walk
};

pmc_evt_code pmc_events_sc_detection[8] = {
	evt_l2_all_data, 	/* L1 miss */
	evt_l2_data_misses,	/* L2 miss */
	evt_l3_miss_data,	/* LLC miss */
	evt_l2_wb,		/* L2 WB */
	evt_l2_in_all,		/* L2 Lines */
	tlb_page_walk		/* TLB L2 Miss */
};

u64 reset_period = (~0xFFFFFFFFULL & (BIT_ULL(48) - 1));