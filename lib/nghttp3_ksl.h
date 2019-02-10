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
#ifndef NGHTTP3_KSL_H
#define NGHTTP3_KSL_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdlib.h>

#include <nghttp3/nghttp3.h>

/*
 * Skip List using single key instead of range.
 */

#define NGHTTP3_KSL_DEGR 8
/* NGHTTP3_KSL_MAX_NBLK is the maximum number of nodes which a single
   block can contain. */
#define NGHTTP3_KSL_MAX_NBLK (2 * NGHTTP3_KSL_DEGR - 1)
/* NGHTTP3_KSL_MIN_NBLK is the minimum number of nodes which a single
   block other than root must contains. */
#define NGHTTP3_KSL_MIN_NBLK (NGHTTP3_KSL_DEGR - 1)

/*
 * nghttp3_ksl_key represents key in nghttp3_ksl.
 */
typedef union {
  int64_t i;
  const void *ptr;
} nghttp3_ksl_key;

struct nghttp3_ksl_node;
typedef struct nghttp3_ksl_node nghttp3_ksl_node;

struct nghttp3_ksl_blk;
typedef struct nghttp3_ksl_blk nghttp3_ksl_blk;

/*
 * nghttp3_ksl_node is a node which contains either nghttp3_ksl_blk or
 * opaque data.  If a node is an internal node, it contains
 * nghttp3_ksl_blk.  Otherwise, it has data.  The invariant is that
 * the key of internal node dictates the maximum key in its
 * descendants, and the corresponding leaf node must exist.
 */
struct nghttp3_ksl_node {
  nghttp3_ksl_key key;
  union {
    nghttp3_ksl_blk *blk;
    void *data;
  };
};

/*
 * nghttp3_ksl_blk contains nghttp3_ksl_node objects.
 */
struct nghttp3_ksl_blk {
  /* next points to the next block if leaf field is nonzero. */
  nghttp3_ksl_blk *next;
  /* prev points to the previous block if leaf field is nonzero. */
  nghttp3_ksl_blk *prev;
  /* n is the number of nodes this object contains in nodes. */
  size_t n;
  /* leaf is nonzero if this block contains leaf nodes. */
  int leaf;
  nghttp3_ksl_node nodes[NGHTTP3_KSL_MAX_NBLK];
};

/*
 * nghttp3_ksl_compar is a function type which returns nonzero if key
 * |lhs| should be placed before |rhs|.  It returns 0 otherwise.
 */
typedef int (*nghttp3_ksl_compar)(const nghttp3_ksl_key *lhs,
                                  const nghttp3_ksl_key *rhs);

struct nghttp3_ksl_it;
typedef struct nghttp3_ksl_it nghttp3_ksl_it;

/*
 * nghttp3_ksl_it is a forward iterator to iterate nodes.
 */
struct nghttp3_ksl_it {
  const nghttp3_ksl_blk *blk;
  size_t i;
  nghttp3_ksl_compar compar;
  nghttp3_ksl_key inf_key;
};

struct nghttp3_ksl;
typedef struct nghttp3_ksl nghttp3_ksl;

/*
 * nghttp3_ksl is a deterministic paged skip list.
 */
struct nghttp3_ksl {
  /* head points to the root block. */
  nghttp3_ksl_blk *head;
  /* front points to the first leaf block. */
  nghttp3_ksl_blk *front;
  /* back points to the last leaf block. */
  nghttp3_ksl_blk *back;
  nghttp3_ksl_compar compar;
  nghttp3_ksl_key inf_key;
  size_t n;
  nghttp3_mem *mem;
};

/*
 * nghttp3_ksl_init initializes |ksl|.  |compar| specifies compare
 * function.  |inf_key| specifies the "infinite" key.
 *
 * It returns 0 if it succeeds, or one of the following negative error
 * codes:
 *
 * NGHTTP3_ERR_NOMEM
 *   Out of memory.
 */
int nghttp3_ksl_init(nghttp3_ksl *ksl, nghttp3_ksl_compar compar,
                     const nghttp3_ksl_key *inf_key, nghttp3_mem *mem);

/*
 * nghttp3_ksl_free frees resources allocated for |ksl|.  If |ksl| is
 * NULL, this function does nothing.  It does not free the memory
 * region pointed by |ksl| itself.
 */
void nghttp3_ksl_free(nghttp3_ksl *ksl);

/*
 * nghttp3_ksl_insert inserts |key| with its associated |data|.  On
 * successful insertion, the iterator points to the inserted node is
 * stored in |*it|.
 *
 * This function assumes that |key| does not exist in |ksl|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGHTTP3_ERR_NOMEM
 *   Out of memory.
 */
int nghttp3_ksl_insert(nghttp3_ksl *ksl, nghttp3_ksl_it *it,
                       const nghttp3_ksl_key *key, void *data);

/*
 * nghttp3_ksl_remove removes the |key| from |ksl|.  It assumes such
 * the key is included in |ksl|.
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
int nghttp3_ksl_remove(nghttp3_ksl *ksl, nghttp3_ksl_it *it,
                       const nghttp3_ksl_key *key);

/*
 * nghttp3_ksl_lower_bound returns the iterator which points to the
 * first node which has the key which is equal to |key| or the last
 * node which satisfies !compar(&node->key, key).  If there is no such
 * node, it returns the iterator which satisfies
 * nghttp3_ksl_it_end(it) != 0.
 */
nghttp3_ksl_it nghttp3_ksl_lower_bound(nghttp3_ksl *ksl,
                                       const nghttp3_ksl_key *key);

/*
 * nghttp3_ksl_update_key replaces the key of nodes which has
 * |old_key| with |new_key|.  |new_key| must be strictly greater than
 * the previous node and strictly smaller than the next node.
 */
void nghttp3_ksl_update_key(nghttp3_ksl *ksl, const nghttp3_ksl_key *old_key,
                            const nghttp3_ksl_key *new_key);

/*
 * nghttp3_ksl_begin returns the iterator which points to the first
 * node.  If there is no node in |ksl|, it returns the iterator which
 * satisfies nghttp3_ksl_it_end(it) != 0.
 */
nghttp3_ksl_it nghttp3_ksl_begin(const nghttp3_ksl *ksl);

/*
 * nghttp3_ksl_end returns the iterator which points to the node
 * following the last node.  The returned object satisfies
 * nghttp3_ksl_it_end().  If there is no node in |ksl|, it returns the
 * iterator which satisfies nghttp3_ksl_it_begin(it) != 0.
 */
nghttp3_ksl_it nghttp3_ksl_end(const nghttp3_ksl *ksl);

/*
 * nghttp3_ksl_len returns the number of elements stored in |ksl|.
 */
size_t nghttp3_ksl_len(nghttp3_ksl *ksl);

/*
 * nghttp3_ksl_clear removes all elements stored in |ksl|.
 */
void nghttp3_ksl_clear(nghttp3_ksl *ksl);

/*
 * nghttp3_ksl_print prints its internal state in stderr.  This
 * function should be used for the debugging purpose only.
 */
void nghttp3_ksl_print(nghttp3_ksl *ksl);

/*
 * nghttp3_ksl_it_init initializes |it|.
 */
void nghttp3_ksl_it_init(nghttp3_ksl_it *it, const nghttp3_ksl_blk *blk,
                         size_t i, nghttp3_ksl_compar compar,
                         const nghttp3_ksl_key *inf_key);

/*
 * nghttp3_ksl_it_get returns the data associated to the node which
 * |it| points to.  If this function is called when
 * nghttp3_ksl_it_end(it) returns nonzero, it returns NULL.
 */
void *nghttp3_ksl_it_get(const nghttp3_ksl_it *it);

/*
 * nghttp3_ksl_it_next advances the iterator by one.  It is undefined
 * if this function is called when nghttp3_ksl_it_end(it) returns
 * nonzero.
 */
void nghttp3_ksl_it_next(nghttp3_ksl_it *it);

/*
 * nghttp3_ksl_it_prev moves backward the iterator by one.  It is
 * undefined if this function is called when nghttp3_ksl_it_begin(it)
 * returns nonzero.
 */
void nghttp3_ksl_it_prev(nghttp3_ksl_it *it);

/*
 * nghttp3_ksl_it_end returns nonzero if |it| points to the beyond the
 * last node.
 */
int nghttp3_ksl_it_end(const nghttp3_ksl_it *it);

/*
 * nghttp3_ksl_it_begin returns nonzero if |it| points to the first
 * node.  |it| might satisfy both nghttp3_ksl_it_begin(&it) and
 * nghttp3_ksl_it_end(&it) if the skip list has no node.
 */
int nghttp3_ksl_it_begin(const nghttp3_ksl_it *it);

/*
 * nghttp3_ksl_key returns the key of the node which |it| points to.
 * It is OK to call this function when nghttp3_ksl_it_end(it) returns
 * nonzero.  In this case, this function returns inf_key.
 */
nghttp3_ksl_key nghttp3_ksl_it_key(const nghttp3_ksl_it *it);

/*
 * nghttp3_ksl_key_i is a convenient function which initializes |key|
 * with |i| and returns |key|.
 */
nghttp3_ksl_key *nghttp3_ksl_key_i(nghttp3_ksl_key *key, int64_t i);

/*
 * nghttp3_ksl_key_ptr is a convenient function which initializes
 * |key| with |ptr| and returns |key|.
 */
nghttp3_ksl_key *nghttp3_ksl_key_ptr(nghttp3_ksl_key *key, const void *ptr);

#endif /* NGHTTP3_KSL_H */
