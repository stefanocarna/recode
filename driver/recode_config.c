#include "recode.h"
#include "pmu/pmu.h"
#include "recode_config.h"
#include "pmu/pmc_events.h"

#define SAMPLING_PERIOD (BIT_ULL(28) - 1)

// u64 __read_mostly gbl_reset_period = PMC_TRIM(~SAMPLING_PERIOD);
u64 __read_mostly gbl_reset_period = SAMPLING_PERIOD;
unsigned __read_mostly gbl_fixed_pmc_pmi = 2; // PMC with PMI active

unsigned params_cpl_os = 1;
unsigned params_cpl_usr = 1;

enum recode_pmi_vector recode_pmi_vector = NMI;

pmc_evt_code pmc_events_management[8] = {
	// MTA Level 0
	{ evt_ur_retire_slots },
	{ evt_ui_any },
	{ evt_im_recovery_cycles },
	{ evt_iund_core },
};

pmc_evt_code pmc_events_tma_l0[8] = { { evt_ur_retire_slots },
				      { evt_ui_any },
				      { evt_im_recovery_cycles },
				      { evt_iund_core },
				      { evt_null } };

pmc_evt_code pmc_events_tma_l1[8] = {
	// TMA Level 0
	{ evt_ur_retire_slots },
	{ evt_ui_any },
	{ evt_im_recovery_cycles },
	{ evt_iund_core },

	// #Memory_Bound_Fraction
	{ evt_ca_stalls_mem_any },
	{ evt_ea_bound_on_stores },
	// #Core_Bound_Cycles
	{ evt_ea_exe_bound_0_ports },
	{ evt_ea_1_ports_util }
};

pmc_evt_code pmc_events_tma_l2[8] = {

};
