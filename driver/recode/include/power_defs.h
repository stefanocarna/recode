#ifndef _PLATFORM_DEFS_H_INCLUDED_
#define _PLATFORM_DEFS_H_INCLUDED_

#include <linux/types.h>

/*
   * For each machine that is used, one needs to define
   *  NUMBER_OF_SOCKETS: the number of sockets the machine has
   *  CORES_PER_SOCKET: the number of cores per socket
   *  CACHE_LINE_SIZE
   *  NOP_DURATION: the duration in cycles of a noop instruction (generally 1 cycle on most small machines)
   *  the_cores - a mapping from the core ids as configured in the OS to physical cores (the OS might not alwas be configured corrrectly)
   *  get_cluster - a function that given a core id returns the socket number ot belongs to
   */

// #ifdef DEFAULT
#define NUMBER_OF_SOCKETS 1
#define CORES_PER_SOCKET CORE_NUM
#define CACHE_LINE_SIZE 64
#define NOP_DURATION 2

// #define USE_HYPERTRHEADS 1

static u8
	__attribute__((unused))
	the_cores[] = { 0,  1,	2,  3,	4,  5,	6,  7,	8,  9,	10, 11,
			12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
			24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
			36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47 };
// #endif


// #if USE_HYPERTRHEADS == 1
// static u8 __attribute__((unused)) the_cores[] = {
// 	0,  1,	2,  3,	4,  5,	6,  7,	8,  9,	20, 21, 22, 23,
// 	24, 25, 26, 27, 28, 29, 10, 11, 12, 13, 14, 15, 16, 17,
// 	18, 19, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
// };
// #else
// static u8 __attribute__((unused)) the_cores[] = {
// 	0,  1,	2,  3,	4,  5,	6,  7,	8,  9,	10, 11, 12, 13,
// 	14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27,
// 	28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
// };
// #endif

#define PREFETCHW(x) asm volatile("prefetchw %0" ::"m"(*(unsigned long *)x))

static inline int get_cluster(int thread_id)
{
	return 0;
	// return thread_id / CORES_PER_SOCKET;
}

#endif /* _PLATFORM_DEFS_H_INCLUDED_ */
