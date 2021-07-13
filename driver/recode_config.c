#include "recode.h"
#include "recode_pmu.h"
#include "recode_config.h"
#include "pmc_events.h"

unsigned params_cpl_os = 0;
unsigned params_cpl_usr = 1;

enum recode_pmi_vector recode_pmi_vector = NMI;

pmc_evt_code pmc_events_management[8] = { // MTA Level 0
	evt_ur_retire_slots,
	evt_ui_any,
	evt_im_recovery_cycles,
	evt_iund_core,
};

pmc_evt_code pmc_events_tma_l0[8] = {
	evt_ur_retire_slots,
	evt_ui_any,
	evt_im_recovery_cycles,
	evt_iund_core,
	evt_null
};

pmc_evt_code pmc_events_tma_l1[8] = {
	// TMA Level 0
	evt_ur_retire_slots,
	evt_ui_any,
	evt_im_recovery_cycles,
	evt_iund_core,
	
	// #Memory_Bound_Fraction
	evt_ca_stalls_mem_any,
	evt_ea_bound_on_stores,
	// #Core_Bound_Cycles
	evt_ea_exe_bound_0_ports,
	evt_ea_1_ports_util	
};

pmc_evt_code pmc_events_tma_l2[8] = {
	
};

// struct pmc_compute_tma {
// 	pmc_evt_code *codes;

// };


u64 reset_period = (~0xFFFFFFULL & (BIT_ULL(48) - 1));