/**
 * @file datatypes/array.h
 *
 * @brief Dynamic array datatype
 *
 * Dynamic array datatype
 *
 * SPDX-FileCopyrightText: 2008-2021 HPDCS Group <rootsim@googlegroups.com>
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef _ARRAY_H
#define _ARRAY_H

#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/string.h>

/// The initial size of dynamic arrays expressed in the number of elements
#define INIT_SIZE_ARRAY 8U
/// The type used to handle dynamic arrays count of elements and capacity
typedef u32 array_count_t;

/**
 * @brief Declares a dynamic array
 * @param type The type of the contained elements
 */
#define array_declare(type)                                                    \
	struct {                                                               \
		type *items;                                                   \
		array_count_t count;                                           \
		array_count_t capacity;                                        \
	}

/**
 * @brief Gets the underlying actual array of elements of a dynamic array
 * @param self The target dynamic array
 * @return a pointer to the underlying array of elements
 *
 * You can use the array to directly index items, but do it at your own risk!
 */
#define array_items(self) ((self).items)

/**
 * @brief Gets the count of contained element in a dynamic array
 * @param self The target dynamic array
 */
#define array_count(self) ((self).count)

/**
 * @brief Gets the current capacity of a dynamic array
 * @param self The target dynamic array
 */
#define array_capacity(self) ((self).capacity)

/**
 * @brief Gets the current capacity of a dynamic array
 * @param self The target dynamic array
 */
#define array_peek(self) (array_items(self)[array_count(self) - 1])
//this isn't checked CARE!
#define array_get_at(self, i) (array_items(self)[(i)])

#define array_is_empty(self) (array_count(self) == 0)

#define array_for_each(x, self)                                                \
	for (x = 0; x < array_count(self); ++x)

#define array_init(self)                                                       \
	__extension__({ array_init_with_size(self, INIT_SIZE_ARRAY); })

#define array_init_with_size(self, s)                                          \
	__extension__({                                                        \
		array_capacity(self) = s;                                      \
		array_items(self) = kvmalloc_array(array_capacity(self),               \
					   sizeof(*array_items(self)), GFP_KERNEL);        \
		if (array_items(self) == NULL)                                 \
			pr_err("array_expand items is NULL\n");                \
		array_count(self) = 0;                                         \
	})

#define array_fini(self) __extension__({ kvfree(array_items(self)); })

#define array_push(self, elem)                                                 \
	__extension__({                                                        \
		array_expand(self);                                            \
		array_items(self)[array_count(self)] = (elem);                 \
		array_count(self)++;                                           \
	})

#define array_pop(self)                                                        \
	__extension__({                                                        \
		array_count(self)--;                                           \
		array_items(self)[array_count(self)];                          \
	})

#define array_add_at(self, i, elem)                                            \
	__extension__({                                                        \
		array_expand(self);                                            \
		memmove(&(array_items(self)[(i) + 1]),                         \
			&(array_items(self)[(i)]),                             \
			sizeof(*array_items(self)) *                           \
				(array_count(self) - (i)));                    \
		array_items(self)[(i)] = (elem);                               \
		array_count(self)++;                                           \
	})

#define array_remove_at(self, i)                                               \
	__extension__({                                                        \
		__typeof__(*array_items(self)) __rmval;                        \
		array_count(self)--;                                           \
		__rmval = array_items(self)[(i)];                              \
		memmove(&(array_items(self)[(i)]),                             \
			&(array_items(self)[(i) + 1]),                         \
			sizeof(*array_items(self)) *                           \
				(array_count(self) - (i)));                    \
		array_shrink(self);                                            \
		__rmval;                                                       \
	})

#define array_truncate_first(self, n)                                          \
	__extension__({                                                        \
		array_count(self) -= n;                                        \
		memmove(array_items(self), &(array_items(self)[n]),            \
			sizeof(*array_items(self)) * (array_count(self)));     \
	})

#define array_shrink(self)                                                     \
	__extension__({                                                        \
		if (unlikely(array_count(self) > INIT_SIZE_ARRAY &&            \
			     array_count(self) * 3 <= array_capacity(self))) { \
			array_count_t old_cap = array_capacity(self);          \
			array_capacity(self) /= 2;                             \
			void *data = kvmalloc_array(array_capacity(self),              \
					    sizeof(*array_items(self)), GFP_KERNEL);       \
			memcpy(array_items(self), data,                        \
			       sizeof(*array_items(self)) * (old_cap));        \
			array_fini(self);                                      \
			array_items(self) = data;                              \
			if (array_items(self) == NULL)                         \
				pr_err("array_expand items is NULL\n");        \
		}                                                              \
	})

#define array_reserve(self, n)                                                 \
	__extension__({                                                        \
		__typeof__(array_count(self)) tcnt = array_count(self) + n;    \
		if (unlikely(tcnt >= array_capacity(self))) {                  \
			array_count_t old_cap = array_capacity(self);          \
			do {                                                   \
				array_capacity(self) *= 2;                     \
			} while (unlikely(tcnt >= array_capacity(self)));      \
			void *data = kvmalloc_array(array_capacity(self),              \
					    sizeof(*array_items(self)), GFP_KERNEL);       \
			memcpy(array_items(self), data,                        \
			       sizeof(*array_items(self)) * (old_cap));        \
			array_fini(self);                                      \
			array_items(self) = data;                              \
			if (array_items(self) == NULL)                         \
				pr_err("array_expand items is NULL\n");        \
		}                                                              \
	})

#define array_expand(self)                                                     \
	__extension__({                                                        \
		if (unlikely(array_count(self) >= array_capacity(self))) {     \
			array_count_t old_cap = array_capacity(self);          \
			if (unlikely(old_cap == 0))                            \
				array_capacity(self) = 1;                      \
			else                                                   \
				array_capacity(self) *= 2;                     \
			void *data = kvmalloc_array(array_capacity(self),              \
					    sizeof(*array_items(self)), GFP_KERNEL);       \
			memcpy(data, array_items(self),                        \
			       sizeof(*array_items(self)) * (old_cap));        \
			array_fini(self);                                      \
			array_items(self) = data;                              \
			if (array_items(self) == NULL)                         \
				pr_err("array_expand items is NULL\n");        \
		}                                                              \
	})

#endif /* _ARRAY_H */
