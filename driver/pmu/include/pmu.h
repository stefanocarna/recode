/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef _PMU_H
#define _PMU_H

#include "pmu_structs.h"

#include <linux/percpu.h>

#define MAX_PARALLEL_HW_EVENTS 32

DECLARE_PER_CPU(struct pmus_metadata, pcpu_pmus_metadata);

extern pmc_ctr __read_mostly gbl_reset_period;
extern uint __read_mostly gbl_fixed_pmc_pmi;
/* SHould be moved to pmu_low */
extern uint __read_mostly gbl_nr_pmc_fixed;
extern uint __read_mostly gbl_nr_pmc_general;

/* This should be changed into a soft state struct */
extern bool __read_mostly pmu_enabled;

void enable_pmcs_local(bool force);
void disable_pmcs_local(bool force);

void enable_pmcs_global(void);
void disable_pmcs_global(void);

void save_and_disable_pmcs_local(bool *state);
void restore_and_enable_pmcs_local(bool *state);

void setup_hw_events_local(struct hw_events *hw_events);
int setup_hw_events_global(struct hw_events *hw_events);

struct hw_events *create_hw_events(pmc_evt_code *codes, uint cnt);
void destroy_hw_events(struct hw_events *hw_events);

void fast_setup_general_pmc_local(struct pmc_evt_sel *pmc_cfgs, uint off,
				  uint cnt);

int setup_pmc_global(pmc_evt_code *pmc_cfgs);

int pmu_global_init(void);
void pmc_global_fini(void);

/* Debug functions */
void debug_pmu_state(void);

u64 compute_hw_events_mask(pmc_evt_code *hw_events_codes, uint cnt);

void update_reset_period_global(u64 reset_period);

void pmudrv_set_state(bool state);

void reset_pmc_global(void);

void reset_hw_events_local(void);

#endif /* _PMU_H */
