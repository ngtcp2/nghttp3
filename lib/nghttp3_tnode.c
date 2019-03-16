/*
 * nghttp3
 *
 * Copyright (c) 2019 nghttp3 contributors
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
#include "nghttp3_tnode.h"

#include <assert.h>

#include "nghttp3_macro.h"
#include "nghttp3_stream.h"

static int cycle_less(const nghttp3_pq_entry *lhsx,
                      const nghttp3_pq_entry *rhsx) {
  const nghttp3_tnode *lhs = nghttp3_struct_of(lhsx, nghttp3_tnode, pe);
  const nghttp3_tnode *rhs = nghttp3_struct_of(rhsx, nghttp3_tnode, pe);

  if (lhs->cycle == rhs->cycle) {
    return lhs->seq < rhs->seq;
  }

  return rhs->cycle - lhs->cycle <= NGHTTP3_TNODE_MAX_CYCLE_GAP;
}

void nghttp3_tnode_init(nghttp3_tnode *tnode, nghttp3_id_type id_type,
                        int64_t id, uint64_t seq, uint32_t weight,
                        nghttp3_tnode *parent, const nghttp3_mem *mem) {
  nghttp3_pq_init(&tnode->pq, cycle_less, mem);

  tnode->pe.index = NGHTTP3_PQ_BAD_INDEX;
  tnode->id = id;
  tnode->id_type = id_type;
  tnode->seq = seq;
  tnode->cycle = 0;
  tnode->pending_penalty = 0;
  tnode->weight = weight;
  tnode->parent = parent;
}

void nghttp3_tnode_free(nghttp3_tnode *tnode) { nghttp3_pq_free(&tnode->pq); }

void nghttp3_tnode_unschedule(nghttp3_tnode *tnode) {
  nghttp3_tnode *parent = tnode->parent;

  if (tnode->pe.index == NGHTTP3_PQ_BAD_INDEX) {
    return;
  }

  nghttp3_pq_remove(&parent->pq, &tnode->pe);

  tnode->pe.index = NGHTTP3_PQ_BAD_INDEX;
}

int nghttp3_tnode_schedule(nghttp3_tnode *tnode, size_t nwrite) {
  nghttp3_tnode *parent = tnode->parent, *top;
  uint64_t cycle;
  uint64_t penalty;

  /* TODO At the moment, we use single level dependency.  All streams
     depend on root. */

  assert(tnode->pe.index == NGHTTP3_PQ_BAD_INDEX);

  if (!nghttp3_pq_empty(&parent->pq)) {
    top = nghttp3_struct_of(nghttp3_pq_top(&parent->pq), nghttp3_tnode, pe);
    cycle = top->cycle;
  } else {
    cycle = tnode->cycle;
  }

  penalty = (uint64_t)nwrite * NGHTTP3_MAX_WEIGHT + tnode->pending_penalty;

  tnode->cycle = cycle + penalty / tnode->weight;
  tnode->pending_penalty = (uint32_t)(penalty % tnode->weight);

  return nghttp3_pq_push(&parent->pq, &tnode->pe);
}

nghttp3_tnode *nghttp3_tnode_get_next(nghttp3_tnode *node) {
  if (nghttp3_pq_empty(&node->pq)) {
    return NULL;
  }

  return nghttp3_struct_of(nghttp3_pq_top(&node->pq), nghttp3_tnode, pe);
}
