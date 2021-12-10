/*   
 *   File: rapl_read.c
 *   Author: Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>
 *   Description: 
 *   rapl_read.c is part of ASCYLIB
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2014 Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>
 *	      	      Distributed Programming Lab (LPD), EPFL
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "power.h"

#include <asm/msr.h>
#include <linux/cpufreq.h>

#define DEFAULT_SOCKET 0

int rapl_cpu_model;
int rapl_msr_fd[NUMBER_OF_SOCKETS];
u64 max_cpu_freq;

#define RAPL_INIT_OFFS 17
bool rapl_initialized[NUMBER_OF_SOCKETS] = {};
bool rapl_dram_counter;
int rapl_resp_core[NUMBER_OF_SOCKETS] = {};
uint32_t rapl_num_active_sockets;

u64 rapl_power_units, rapl_energy_units, rapl_time_units;
u64 rapl_package_before[NUMBER_OF_SOCKETS],
	rapl_package_after[NUMBER_OF_SOCKETS],
	rapl_pp0_before[NUMBER_OF_SOCKETS], rapl_pp0_after[NUMBER_OF_SOCKETS],
	rapl_pp1_before[NUMBER_OF_SOCKETS], rapl_pp1_after[NUMBER_OF_SOCKETS],
	rapl_dram_before[NUMBER_OF_SOCKETS], rapl_dram_after[NUMBER_OF_SOCKETS];
u64 rapl_thermal_spec_power, rapl_minimum_power, rapl_maximum_power,
	rapl_time_window;
u64 rapl_pkg_power_limit_1, rapl_pkg_time_window_1,
	rapl_pkg_power_limit_2, rapl_pkg_time_window_2;
u64 rapl_acc_pkg_throttled_time, rapl_acc_rapl_pp0_throttled_time;

long long rapl_msr_pkg_settings;
int rapl_pp0_policy, rapl_pp1_policy;

u64 rapl_start_ts[NUMBER_OF_SOCKETS], rapl_stop_ts[NUMBER_OF_SOCKETS];

#define FOR_ALL_SOCKETS(s) for (s = 0; s < NUMBER_OF_SOCKETS; s++)

#define FOR_ALL_SELECTED_SOCKETS(socket, s)                                    \
	for (s = 0; s < NUMBER_OF_SOCKETS; s++)                                \
		if (socket == RR_NODE_ALL || s == socket)

#define NON_SELECTED_SOCKETS else

static inline bool rapl_allowed(void)
{
	return rapl_initialized[0];
}

static inline bool rapl_allowed_once(void)
{
	int min_socket = 0;

	if (!rapl_allowed())
		return false;

	while (min_socket < NUMBER_OF_SOCKETS &&
	       !rapl_initialized[min_socket]) {
		min_socket++;
	}

	return min_socket == DEFAULT_SOCKET;
}

void rapl_read_init_all(void)
{
	u64 msr;

	__sync_fetch_and_add(&rapl_num_active_sockets, NUMBER_OF_SOCKETS);

	rapl_dram_counter = true;
	max_cpu_freq = cpufreq_get_hw_max_freq(0) / 1000000; // Getting GHz

	/* Unique Socket (0) */
	rapl_initialized[DEFAULT_SOCKET] = true;

	/* Calculate the units used - 2^(-hw_unit)*/
	rdmsrl(MSR_RAPL_POWER_UNIT, msr);
	rapl_power_units = msr & 0xF;
	rapl_energy_units = (msr >> 8) & 0x1F;
	rapl_time_units = (msr >> 16) & 0xF;

	// /* Show package power info - MSR_RAPL_POWER_UNIT * Value */
	// rdmsrl(MSR_PKG_POWER_INFO, msr);
	// rapl_thermal_spec_power = msr & 0x7FFF; // rapl_power_units
	// rapl_minimum_power = (msr >> 16) & 0x7FFF; // rapl_power_units
	// rapl_maximum_power = (msr >> 32) & 0x7FFF; // rapl_power_units
	// rapl_time_window = (msr >> 48) & 0x7FFF; // rapl_time_units

	// /* Show package power limit */
	// rdmsrl(MSR_PKG_POWER_LIMIT, msr);
	// rapl_msr_pkg_settings = msr;
	// rapl_pkg_power_limit_1 = (msr >> 0) & 0x7FFF; // rapl_power_units
	// rapl_pkg_time_window_1 = (msr >> 17) & 0x007F; // rapl_time_units
	// rapl_pkg_power_limit_2 = (msr >> 32) & 0x7FFF; // rapl_power_units
	// rapl_pkg_time_window_2 = (msr >> 49) & 0x007F;// rapl_time_units
}

void rapl_read_start(void)
{
	u64 msr;

	if (!rapl_allowed())
		return;

	rdmsrl(MSR_PKG_ENERGY_STATUS, msr);
	rapl_package_before[DEFAULT_SOCKET] = msr; // rapl_energy_units

	rdmsrl(MSR_PP0_ENERGY_STATUS, msr);
	rapl_pp0_before[DEFAULT_SOCKET] = msr; // rapl_energy_units

	rdmsrl(MSR_PP0_POLICY, msr);
	rapl_pp0_policy = (int)msr & 0x001F;

	rdmsrl(MSR_PP1_ENERGY_STATUS, msr);
	rapl_pp1_before[DEFAULT_SOCKET] = msr; // rapl_energy_units

	rdmsrl(MSR_PP1_POLICY, msr);
	rapl_pp1_policy = (int)msr & 0x001f;

	/* TODO CHeck compatibility with Intel 10th Gen */
	rdmsrl(MSR_DRAM_ENERGY_STATUS, msr);
	rapl_dram_before[DEFAULT_SOCKET] = msr; // rapl_energy_units

	rapl_start_ts[DEFAULT_SOCKET] = rdtsc_ordered();
}

void rapl_read_stop(void)
{
	u64 msr;

	if (!rapl_allowed())
		return;

	rapl_stop_ts[DEFAULT_SOCKET] = rdtsc_ordered();

	rdmsrl(MSR_PKG_ENERGY_STATUS, msr);
	rapl_package_after[DEFAULT_SOCKET] = msr; // rapl_energy_units

	rdmsrl(MSR_PP0_ENERGY_STATUS, msr);
	rapl_pp0_after[DEFAULT_SOCKET] = msr; // rapl_energy_units

	rdmsrl(MSR_PP1_ENERGY_STATUS, msr);
	rapl_pp1_after[DEFAULT_SOCKET] = msr; // rapl_energy_units

	/* TODO CHeck compatibility with Intel 10th Gen */
	rdmsrl(MSR_DRAM_ENERGY_STATUS, msr);
	rapl_dram_after[DEFAULT_SOCKET] = msr; // rapl_energy_units
}

#define FOR_ALL_SOCKETS_PLUS1(s) for (s = 0; s < NUMBER_OF_SOCKETS + 1; s++)

void rapl_read_stats(struct rapl_stats *s)
{
	int i;
	u64 duration[NUMBER_OF_SOCKETS];
	// u64 duration_s[NUMBER_OF_SOCKETS];
	u64 rapl_package[NUMBER_OF_SOCKETS];
	u64 rapl_pp0[NUMBER_OF_SOCKETS];
	u64 rapl_pp1[NUMBER_OF_SOCKETS];
	u64 rapl_rest[NUMBER_OF_SOCKETS];
	u64 rapl_dram[NUMBER_OF_SOCKETS];

	FOR_ALL_SOCKETS(i)
	{
		duration[i] = rapl_stop_ts[i] - rapl_start_ts[i];
		// duration_s[i] = (double)duration[i] / ((CORE_SPEED_GHZ)*1e9);
		rapl_package[i] = rapl_package_after[i] - rapl_package_before[i];
		rapl_pp0[i] = rapl_pp0_after[i] - rapl_pp0_before[i];
		rapl_pp1[i] = rapl_pp1_after[i] - rapl_pp1_before[i];
		rapl_rest[i] = rapl_package[i] - rapl_pp0[i];
		rapl_dram[i] = rapl_dram_after[i] - rapl_dram_before[i];
	}

	i = 0;
	s->duration[i] = duration[i];
	s->energy_package[i] = rapl_package[i];
	s->energy_pp0[i] = rapl_pp0[i];
	s->energy_pp1[i] = rapl_pp1[i];
	s->energy_rest[i] = rapl_rest[i];
	s->energy_dram[i] = rapl_dram[i];

	s->power_units[i] = rapl_power_units;
	s->energy_units[i] = rapl_energy_units;
	s->time_units[i] = rapl_time_units;

	// if (duration_s > 0) {
	// 	FOR_ALL_SOCKETS_PLUS1(i)
	// 	{
	// 		s->power_package[i] =
	// 			s->energy_package[i] / s->duration[i];
	// 		s->power_pp0[i] = s->energy_pp0[i] / s->duration[i];
	// 		s->power_rest[i] = s->energy_rest[i] / s->duration[i];
	// 		s->power_dram[i] = s->energy_dram[i] / s->duration[i];
	// 		s->power_total[i] = s->energy_total[i] / s->duration[i];
	// 	}
	// }
}

void rapl_read_and_sum_stats(struct rapl_stats *s)
{
	int i;
	u64 duration[NUMBER_OF_SOCKETS];
	// u64 duration_s[NUMBER_OF_SOCKETS];
	u64 rapl_package[NUMBER_OF_SOCKETS];
	u64 rapl_pp0[NUMBER_OF_SOCKETS];
	u64 rapl_pp1[NUMBER_OF_SOCKETS];
	u64 rapl_rest[NUMBER_OF_SOCKETS];
	u64 rapl_dram[NUMBER_OF_SOCKETS];

	FOR_ALL_SOCKETS(i)
	{
		duration[i] = rapl_stop_ts[i] - rapl_start_ts[i];
		// duration_s[i] = (double)duration[i] / ((CORE_SPEED_GHZ)*1e9);
		rapl_package[i] = rapl_package_after[i] - rapl_package_before[i];
		rapl_pp0[i] = rapl_pp0_after[i] - rapl_pp0_before[i];
		rapl_pp1[i] = rapl_pp1_after[i] - rapl_pp1_before[i];
		rapl_rest[i] = rapl_package[i] - rapl_pp0[i];
		rapl_dram[i] = rapl_dram_after[i] - rapl_dram_before[i];
	}

	i = 0;
	s->duration[i] += duration[i];
	s->energy_package[i] += rapl_package[i];
	s->energy_pp0[i] += rapl_pp0[i];
	s->energy_pp1[i] += rapl_pp1[i];
	s->energy_rest[i] += rapl_rest[i];
	s->energy_dram[i] += rapl_dram[i];

	s->power_units[i] += rapl_power_units;
	s->energy_units[i] += rapl_energy_units;
	s->time_units[i] += rapl_time_units;
}
