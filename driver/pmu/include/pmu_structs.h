/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef _PMU_STRUCTS_H
#define _PMU_STRUCTS_H

#include <linux/types.h>

typedef u64 pmc_ctr;

typedef union {
	u32 raw;
	struct {
		u8 evt;
		u8 umask;
		u8 reserved;
		u8 cmask;
	};
} pmc_evt_code;

/* Recode structs */
struct pmc_evt_sel {
	union {
		u64 perf_evt_sel;
		struct {
			u64 evt : 8, umask : 8, usr : 1, os : 1, edge : 1,
				pc : 1, pmi : 1, any : 1, en : 1, inv : 1,
				cmask : 8, reserved : 32;
		};
	};
} __packed;

struct pmcs_collection {
	u64 mask;
	uint cnt;
	/* [ FIXED0 - FIXED1 - ... - GENERAL0 - GENERAL1 - ... ] */
	pmc_ctr pmcs[];
};

struct hw_events {
	u64 mask;
	uint cnt;
	struct pmc_evt_sel cfgs[];
};

struct pmus_metadata {
	u64 fixed_ctrl;
	u64 perf_global_ctrl;
	bool pmcs_active;

	uint pmi_counter;

	u64 ctx_cnt;

	pmc_ctr pmi_reset_value;
	uint pmi_partial_cnt;

	u64 sample_tsc;
	pmc_ctr last_tsc;

	int tma_level;

	uint hw_events_index;
	struct hw_events *hw_events;
	uint multiplexing;
	pmc_ctr *hw_pmcs;
	struct pmcs_collection *pmcs_collection;
};

typedef void pmi_callback(unsigned int cpu, struct pmus_metadata *pmus_metadata);
typedef void hw_events_change_callback(struct hw_events *events);

#endif /* _PMU_STRUCTS_H */
