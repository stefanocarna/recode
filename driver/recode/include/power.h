/*   
 *   File: rapl_read.h
 *   Author: Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>
 *   Description: 
 *   rapl_read.h is part of ASCYLIB
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

#ifndef _RAPL_READ_H_
#define _RAPL_READ_H_

#include "power_defs.h"

#define RR_INIT_ALL() rapl_read_init_all()

#define RR_START() rapl_read_start()

#define RR_STOP() rapl_read_stop()

#define RR_STATS(s) rapl_read_stats(s)

#define RAPL_PRINT_NOT -1L
#define RAPL_PRINT_POW 0L
#define RAPL_PRINT_ENE 1L
#define RAPL_PRINT_BEF_AFT 2L
#define RAPL_PRINT_ALL 3L

#define RR_NODE_ALL -1

/* RAPL UNIT BITMASK */
#define POWER_UNIT_OFFSET 0
#define POWER_UNIT_MASK 0x0F

#define ENERGY_UNIT_OFFSET 0x08
#define ENERGY_UNIT_MASK 0x1F00

#define TIME_UNIT_OFFSET 0x10
#define TIME_UNIT_MASK 0xF000

#define CPU_SANDYBRIDGE 42
#define CPU_SANDYBRIDGE_EP 45
#define CPU_IVYBRIDGE 58
#define CPU_IVYBRIDGE_EP 62
#define CPU_HASWELL 60

int detect_cpu(void);

void rapl_read_init_all(void);
void rapl_read_start(void);
void rapl_read_stop(void);

struct rapl_stats {
	u64 duration[NUMBER_OF_SOCKETS];

	u64 energy_package[NUMBER_OF_SOCKETS];
	u64 energy_pp0[NUMBER_OF_SOCKETS];
	u64 energy_pp1[NUMBER_OF_SOCKETS];
	u64 energy_rest[NUMBER_OF_SOCKETS];
	u64 energy_dram[NUMBER_OF_SOCKETS];
	// u64 energy_total[NUMBER_OF_SOCKETS];

	u64 power_units[NUMBER_OF_SOCKETS];
	u64 energy_units[NUMBER_OF_SOCKETS];
	u64 time_units[NUMBER_OF_SOCKETS];

	// u64 power_package[NUMBER_OF_SOCKETS];
	// u64 power_pp0[NUMBER_OF_SOCKETS];
	// u64 power_pp1[NUMBER_OF_SOCKETS];
	// u64 power_rest[NUMBER_OF_SOCKETS];
	// u64 power_dram[NUMBER_OF_SOCKETS];
	// u64 power_total[NUMBER_OF_SOCKETS];
};

void rapl_read_stats(struct rapl_stats *s);
void rapl_read_and_sum_stats(struct rapl_stats *s);

#endif /* _RAPL_READ_H_ */
