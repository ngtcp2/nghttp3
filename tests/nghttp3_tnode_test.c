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
#include "nghttp3_tnode_test.h"

#include <stdio.h>

#include <CUnit/CUnit.h>

#include "nghttp3_tnode.h"
#include "nghttp3_macro.h"
#include "nghttp3_test_helper.h"

static int cycle_less(const nghttp3_pq_entry *lhsx,
                      const nghttp3_pq_entry *rhsx) {
  const nghttp3_tnode *lhs = nghttp3_struct_of(lhsx, nghttp3_tnode, pe);
  const nghttp3_tnode *rhs = nghttp3_struct_of(rhsx, nghttp3_tnode, pe);

  if (lhs->cycle == rhs->cycle) {
    return lhs->id < rhs->id;
  }

  return rhs->cycle - lhs->cycle <= NGHTTP3_TNODE_MAX_CYCLE_GAP;
}

void test_nghttp3_tnode_schedule(void) {
  const nghttp3_mem *mem = nghttp3_mem_default();
  nghttp3_tnode node, node2;
  nghttp3_pq pq;
  int rv;
  nghttp3_pq_entry *ent;
  nghttp3_tnode *p;

  /* Schedule node with incremental enabled */
  nghttp3_tnode_init(&node, 0);
  node.pri.inc = 1;

  nghttp3_pq_init(&pq, cycle_less, mem);

  rv = nghttp3_tnode_schedule(&node, &pq, 0);

  CU_ASSERT(0 == rv);
  CU_ASSERT(0 == node.cycle);

  /* Schedule another node */
  nghttp3_tnode_init(&node2, 1);
  node.pri.inc = 1;

  rv = nghttp3_tnode_schedule(&node2, &pq, 0);

  CU_ASSERT(0 == rv);

  /* Rescheduling node with nwrite > 0 */

  rv = nghttp3_tnode_schedule(&node, &pq, 1000);

  CU_ASSERT(0 == rv);
  CU_ASSERT(1 == node.cycle);

  /* Rescheduling node with nwrite == 0 */

  rv = nghttp3_tnode_schedule(&node, &pq, 0);

  CU_ASSERT(0 == rv);
  CU_ASSERT(1 == node.cycle);

  nghttp3_pq_free(&pq);

  /* Schedule node without incremental */
  nghttp3_tnode_init(&node, 0);

  nghttp3_pq_init(&pq, cycle_less, mem);

  rv = nghttp3_tnode_schedule(&node, &pq, 0);

  CU_ASSERT(0 == rv);
  CU_ASSERT(0 == node.cycle);

  /* Schedule another node */
  nghttp3_tnode_init(&node2, 1);

  rv = nghttp3_tnode_schedule(&node2, &pq, 0);

  CU_ASSERT(0 == rv);

  /* Rescheduling node with nwrite > 0 */

  rv = nghttp3_tnode_schedule(&node, &pq, 1000);

  CU_ASSERT(0 == rv);
  CU_ASSERT(0 == node.cycle);

  /* Rescheduling node with nwrit == 0 */

  rv = nghttp3_tnode_schedule(&node, &pq, 0);

  CU_ASSERT(0 == rv);
  CU_ASSERT(0 == node.cycle);

  nghttp3_pq_free(&pq);

  /* Stream with lower stream ID takes precedence */
  nghttp3_pq_init(&pq, cycle_less, mem);

  nghttp3_tnode_init(&node2, 1);

  rv = nghttp3_tnode_schedule(&node2, &pq, 0);

  CU_ASSERT(0 == rv);

  nghttp3_tnode_init(&node, 0);

  rv = nghttp3_tnode_schedule(&node, &pq, 0);

  CU_ASSERT(0 == rv);

  ent = nghttp3_pq_top(&pq);

  p = nghttp3_struct_of(ent, nghttp3_tnode, pe);

  CU_ASSERT(0 == p->id);

  nghttp3_pq_free(&pq);

  /* Check the same reversing push order */
  nghttp3_pq_init(&pq, cycle_less, mem);

  nghttp3_tnode_init(&node, 0);

  rv = nghttp3_tnode_schedule(&node, &pq, 0);

  CU_ASSERT(0 == rv);

  nghttp3_tnode_init(&node2, 1);

  rv = nghttp3_tnode_schedule(&node2, &pq, 0);

  CU_ASSERT(0 == rv);

  ent = nghttp3_pq_top(&pq);

  p = nghttp3_struct_of(ent, nghttp3_tnode, pe);

  CU_ASSERT(0 == p->id);

  nghttp3_pq_free(&pq);
}
