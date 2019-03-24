/*
 * nghttp3
 *
 * Copyright (c) 2019 nghttp3 contributors
 * Copyright (c) 2018 ngtcp2 contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef NGHTTP3_PSL_H
#define NGHTTP3_PSL_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdlib.h>

#include <nghttp3/nghttp3.h>

#include "nghttp3_range.h"

/*
 * Skip List implementation inspired by
 * https://github.com/jabr/olio/blob/master/skiplist.c
 */

#define NGHTTP3_PSL_DEGR 8
/* NGHTTP3_PSL_MAX_NBLK is the maximum number of nodes which a single
   block can contain. */
#define NGHTTP3_PSL_MAX_NBLK (2 * NGHTTP3_PSL_DEGR - 1)
/* NGHTTP3_PSL_MIN_NBLK is the minimum number of nodes which a single
   block other than root must contains. */
#define NGHTTP3_PSL_MIN_NBLK (NGHTTP3_PSL_DEGR - 1)

struct nghttp3_psl_node;
typedef struct nghttp3_psl_node nghttp3_psl_node;

struct nghttp3_psl_blk;
typedef struct nghttp3_psl_blk nghttp3_psl_blk;

/*
 * nghttp3_psl_node is a node which contains either nghttp3_psl_blk or
 * opaque data.  If a node is an internal node, it contains
 * nghttp3_psl_blk.  Otherwise, it has data.  The invariant is that
 * the range of internal node dictates the maximum range in its
 * descendants, and the corresponding leaf node must exist.
 */
struct nghttp3_psl_node {
  nghttp3_range range;
  union {
    nghttp3_psl_blk *blk;
    void *data;
  };
};

/*
 * nghttp3_psl_blk contains nghttp3_psl_node objects.
 */
struct nghttp3_psl_blk {
  /* next points to the next block if leaf field is nonzero. */
  nghttp3_psl_blk *next;
  /* n is the number of nodes this object contains in nodes. */
  size_t n;
  /* leaf is nonzero if this block contains leaf nodes. */
  int leaf;
  nghttp3_psl_node nodes[NGHTTP3_PSL_MAX_NBLK];
};

struct nghttp3_psl_it;
typedef struct nghttp3_psl_it nghttp3_psl_it;

/*
 * nghttp3_psl_it is a forward iterator to iterate nodes.
 */
struct nghttp3_psl_it {
  const nghttp3_psl_blk *blk;
  size_t i;
};

struct nghttp3_psl;
typedef struct nghttp3_psl nghttp3_psl;

/*
 * nghttp3_psl is a deterministic paged skip list.
 */
struct nghttp3_psl {
  /* head points to the root block. */
  nghttp3_psl_blk *head;
  /* front points to the first leaf block. */
  nghttp3_psl_blk *front;
  size_t n;
  const nghttp3_mem *mem;
};

/*
 * nghttp3_psl_init initializes |psl|.
 *
 * It returns 0 if it succeeds, or one of the following negative error
 * codes:
 *
 * NGHTTP3_ERR_NOMEM
 *   Out of memory.
 */
int nghttp3_psl_init(nghttp3_psl *psl, const nghttp3_mem *mem);

/*
 * nghttp3_psl_free frees resources allocated for |psl|.  If |psl| is
 * NULL, this function does nothing.  It does not free the memory
 * region pointed by |psl| itself.
 */
void nghttp3_psl_free(nghttp3_psl *psl);

/*
 * nghttp3_psl_insert inserts |range| with its associated |data|.  On
 * successful insertion, the iterator points to the inserted node is
 * stored in |*it|.
 *
 * This function assumes that the existing ranges do not intersect
 * with |range|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGHTTP3_ERR_NOMEM
 *   Out of memory.
 */
int nghttp3_psl_insert(nghttp3_psl *psl, nghttp3_psl_it *it,
                       const nghttp3_range *range, void *data);

/*
 * nghttp3_psl_remove removes the |range| from |psl|.  It assumes such
 * the range is included in |psl|.
 *
 * This function assigns the iterator to |*it|, which points to the
 * node which is located at the right next of the removed node if |it|
 * is not NULL.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGHTTP3_ERR_NOMEM
 *   Out of memory.
 */
int nghttp3_psl_remove(nghttp3_psl *psl, nghttp3_psl_it *it,
                       const nghttp3_range *range);

/*
 * nghttp3_psl_update_range replaces the range of nodes which has
 * |old_range| with |new_range|.  |old_range| must include
 * |new_range|.
 */
void nghttp3_psl_update_range(nghttp3_psl *psl, const nghttp3_range *old_range,
                              const nghttp3_range *new_range);

/*
 * nghttp3_psl_lower_bound returns the iterator which points to the
 * first node whose range intersects with |range|.  If there is no
 * such node, it returns the iterator which satisfies
 * nghttp3_psl_it_end(it) != 0.
 */
nghttp3_psl_it nghttp3_psl_lower_bound(nghttp3_psl *psl,
                                       const nghttp3_range *range);

/*
 * nghttp3_psl_begin returns the iterator which points to the first
 * node.  If there is no node in |psl|, it returns the iterator which
 * satisfies nghttp3_psl_it_end(it) != 0.
 */
nghttp3_psl_it nghttp3_psl_begin(const nghttp3_psl *psl);

/*
 * nghttp3_psl_len returns the number of elements stored in |ksl|.
 */
size_t nghttp3_psl_len(nghttp3_psl *psl);

/*
 * nghttp3_psl_print prints its internal state in stderr.  This
 * function should be used for the debugging purpose only.
 */
void nghttp3_psl_print(nghttp3_psl *psl);

/*
 * nghttp3_psl_it_init initializes |it|.
 */
void nghttp3_psl_it_init(nghttp3_psl_it *it, const nghttp3_psl_blk *blk,
                         size_t i);

/*
 * nghttp3_psl_it_get returns the data associated to the node which
 * |it| points to.  If this function is called when
 * nghttp3_psl_it_end(it) returns nonzero, it returns NULL.
 */
void *nghttp3_psl_it_get(const nghttp3_psl_it *it);

/*
 * nghttp3_psl_it_next advances the iterator by one.  It is undefined
 * if this function is called when nghttp3_psl_it_end(it) returns
 * nonzero.
 */
void nghttp3_psl_it_next(nghttp3_psl_it *it);

/*
 * nghttp3_psl_it_end returns nonzero if |it| points to the beyond the
 * last node.
 */
int nghttp3_psl_it_end(const nghttp3_psl_it *it);

/*
 * nghttp3_psl_range returns the range of the node which |it| points
 * to.  It is OK to call this function when nghttp3_psl_it_end(it)
 * returns nonzero.  In this case, this function returns {UINT64_MAX,
 * UINT64_MAX}.
 */
nghttp3_range nghttp3_psl_it_range(const nghttp3_psl_it *it);

#endif /* NGHTTP3_PSL_H */
