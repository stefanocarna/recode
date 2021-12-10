#include <linux/sort.h>

#include "array.h"
#include "tma_scheduler.h"

typedef struct bucket weight_t;
typedef array_declare(weight_t) w_array_t;
typedef array_declare(w_array_t) w2d_array_t;
typedef array_declare(w2d_array_t) w3d_array_t;

/* Bin Packing Code */

static int compare_descending(const void *lhs, const void *rhs)
{
	int lhs_integer = ((const struct bucket *)(lhs))->k;
	int rhs_integer = ((const struct bucket *)(rhs))->k;

	if (lhs_integer < rhs_integer)
		return -1;
	if (lhs_integer > rhs_integer)
		return 1;
	return 0;
}

static void sort_descending(struct bucket *buckets, size_t size)
{
	sort(buckets, size, sizeof(struct bucket), &compare_descending, NULL);
}

static int firstFit(struct bucket *buckets, size_t size, int cap)
{
	// Initialize result (Count of bins)
	int i, j;
	int nr_bin = 0;

	// Create an array to store remaining space in bins
	// there can be at most n bins
	int *rem = kmalloc_array(size, sizeof(int), GFP_KERNEL);
	// struct bin *bins = calloc(sizeof(struct bin), n);
	// struct bin_sol *bin_sol = malloc(sizeof(*bin_sol));

	// Place items one by one
	for (i = 0; i < size; i++) {
		// Find the first bin that can accommodate
		// weight[i]
		for (j = 0; j < nr_bin; j++) {
			if (rem[j] >= buckets[i].k) {
				rem[j] = rem[j] - buckets[i].k;
				// pr_info("BB %u <- %u\n", j, buckets[i].k);
				// bins[j].elems[bins[j].nr++] = weight[i];
				break;
			}
		}

		// If no bin could accommodate weight[i]
		if (j == nr_bin) {
			rem[nr_bin] = cap - buckets[i].k;
			// pr_info("BB %u <- %u\n", nr_bin, buckets[i].k);

			// bins[res].elems[bins[res].nr++] = weight[i];

			nr_bin++;
		}
	}

	// bin_sol->bins = bins;
	// bin_sol->nr = res;

	kfree(rem);
	return nr_bin;
}

int min_bins(struct bucket *buckets, size_t size, int cap)
{
	/* Require array to be sorted in descending order */
	sort_descending(buckets, size);

	return firstFit(buckets, size, cap);
}

/* K-partitioning Code  */

static void w2d_array_free(w2d_array_t ws)
{
	for (array_count_t i = 0; i < array_count(ws); ++i)
		array_fini(array_get_at(ws, i));

	array_fini(ws);
}

static void w3d_array_free(w3d_array_t wss)
{
	for (array_count_t i = 0; i < array_count(wss); ++i)
		w2d_array_free(array_get_at(wss, i));

	array_fini(wss);
}

static int w_array_sum(const w_array_t ws)
{
	int ret = 0;
	for (array_count_t i = 0; i < array_count(ws); ++i)
		ret += array_get_at(ws, i).k;

	return ret;
}

static w2d_array_t copy_2d_array(const w2d_array_t ws)
{
	w2d_array_t ret;
	array_init_with_size(ret, array_capacity(ws));

	for (array_count_t i = 0; i < array_count(ws); ++i) {
		w_array_t *a = &array_get_at(ws, i);
		w_array_t ins;
		array_init_with_size(ins, array_capacity(*a));
		memcpy(array_items(ins), array_items(*a),
		       sizeof(*array_items(ins)) * array_count(*a));
		array_count(ins) = array_count(*a);
		array_push(ret, ins);
	}
	return ret;
}

w3d_array_t partition_k_cap(const weight_t *c, array_count_t s, array_count_t k,
			    int max_w)
{
	w3d_array_t parts;

	array_init(parts);

	if (s == 1) {
		w_array_t r1;

		array_init(r1);
		array_push(r1, *c);

		w2d_array_t r2;

		array_init(r2);
		array_push(r2, r1);

		array_push(parts, r2);
		return parts;
	}

	w3d_array_t prev = partition_k_cap(c + 1, s - 1, k, max_w);

	for (array_count_t i = 0; i < array_count(prev); ++i) {
		w2d_array_t *smaller = &array_get_at(prev, i);

		array_count_t l = array_count(*smaller);
		// insert `first` in each of the subpartition's subsets
		for (array_count_t j = 0; j < l; ++j) {
			w_array_t *t = &array_get_at(*smaller, j);

			if (w_array_sum(*t) + (*c).k > max_w)
				continue;

			w2d_array_t ins = copy_2d_array(*smaller);
			w_array_t *r1 = &array_get_at(ins, j);

			array_add_at(*r1, 0, *c);
			array_push(parts, ins);
		}
		// put `first` in its own subset
		if (l == k)
			continue;

		w2d_array_t ins = copy_2d_array(*smaller);

		w_array_t r1;

		array_init(r1);
		array_push(r1, *c);

		array_add_at(ins, 0, r1);
		array_push(parts, ins);
	}
	w3d_array_free(prev);

	return parts;
}

size_t compute_k_partitions_min_max_cap(struct csched **av_cs_p,
					struct bucket *buckets, size_t size,
					int min_cap, int max_cap)
{
	int i, k;
	int w_sum = 0;
	struct csched *av_cs;
	w3d_array_t parts;

	for (i = 0; i < size; ++i)
		w_sum += buckets[i].k;

	k = w_sum / min_cap;

	/* No partition available */
	if (k < 2) {
	no_part:
		av_cs = kmalloc(sizeof(*av_cs), GFP_KERNEL);
		if (!av_cs)
			return 0;

		av_cs[0].nr_parts = 1;
		av_cs[0].parts =
			kmalloc(sizeof(struct csched_part), GFP_KERNEL);
		/* TODO Check mem allocation */

		av_cs[0].parts[0].nr_groups = size;
		av_cs[0].parts[0].groups = kmalloc_array(
			size, sizeof(*av_cs->parts->groups), GFP_KERNEL);

		for (i = 0; i < size; ++i)
			av_cs[0].parts[0].groups[i] =
				(struct group_entity *)buckets[i].payload;

		av_cs_p[0] = av_cs;
		return 1;
	}

	/* Try to find some interesting partitions */

less_part:
	/* Getting sets that may contain |partition| < cap */
	parts = partition_k_cap(buckets, size, k, max_cap);

	pr_info("K %u Sp = %u\n", k, array_count(parts));

	/* Filter unwanted partition */
	for (array_count_t i = 0; i < array_count(parts); ++i) {
		w2d_array_t res = array_get_at(parts, i);

		array_for_each(j, res) {
			if (w_array_sum(array_get_at(res, j)) < min_cap) {
				res = array_remove_at(parts, i);
				w2d_array_free(res);
				--i;
				break;
			}
		}
	}

	if (array_count(parts) == 0) {
		w3d_array_free(parts);

		if (--k < 2)
			goto no_part;
		else
			goto less_part;
	}

	/* Assemble sets into required format */

	av_cs = kmalloc_array(array_count(parts), sizeof(*av_cs), GFP_KERNEL);
	if (!av_cs) {
		w3d_array_free(parts);
		return 0;
	}

	array_for_each(i, parts) {
		w2d_array_t res = array_get_at(parts, i);

		av_cs[i].parts =
			kmalloc_array(array_count(res),
				      sizeof(struct csched_part), GFP_KERNEL);

		av_cs[i].nr_parts = array_count(res);

		// pr_info("-------\n");

		array_for_each(j, res) {
			w_array_t a = array_get_at(res, j);

			av_cs[i].parts[j].cpu_weight = 0;

			av_cs[i].parts[j].groups =
				kmalloc_array(array_count(a),
					      sizeof(*av_cs[i].parts[j].groups),
					      GFP_KERNEL);

			av_cs[i].parts[j].nr_groups = array_count(a);

			// pr_info("P: %u\n", array_count(a));
			array_for_each(l, a) {
				av_cs[i].parts[j].groups[l] =
					((struct group_entity *)array_get_at(a,
									     l)
						 .payload);

				av_cs[i].parts[j].cpu_weight +=
					array_get_at(a, l).k;
				// pr_info("- %u", array_get_at(a, l).k);
			}
		}
	}

	w3d_array_free(parts);

	*av_cs_p = av_cs;
	return array_count(parts);
}
