#include "recode.h"
#include "pmu/pmu.h"
#include "recode_config.h"
// #include "pmu/hw_events.h"

#define SAMPLING_PERIOD (BIT_ULL(28) - 1)

// u64 __read_mostly gbl_reset_period = PMC_TRIM(~SAMPLING_PERIOD);
u64 __read_mostly gbl_reset_period = SAMPLING_PERIOD;
unsigned __read_mostly gbl_fixed_pmc_pmi = 2; // PMC with PMI active

unsigned params_cpl_os = 1;
unsigned params_cpl_usr = 1;

enum recode_pmi_vector recode_pmi_vector = NMI;