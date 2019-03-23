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

#include <CUnit/CUnit.h>

#include "nghttp3_tnode.h"
#include "nghttp3_macro.h"
#include "nghttp3_test_helper.h"

void test_nghttp3_tnode_mutation(void) {
  const nghttp3_mem *mem = nghttp3_mem_default();
  nghttp3_tnode nodes[6];
  nghttp3_tnode *root = &nodes[0], *a = &nodes[1], *b = &nodes[2],
                *c = &nodes[3], *d = &nodes[4], *e = &nodes[5];
  nghttp3_node_id rnid, snid;
  int rv;

  nghttp3_node_id_init(&rnid, NGHTTP3_NODE_ID_TYPE_ROOT, 0);
  nghttp3_node_id_init(&snid, NGHTTP3_NODE_ID_TYPE_STREAM, 0);

  /* Insert a node to empty root */
  nghttp3_tnode_init(root, &rnid, 0, NGHTTP3_DEFAULT_WEIGHT, NULL, mem);
  nghttp3_tnode_init(a, &snid, 0, 12, NULL, mem);

  nghttp3_tnode_insert(a, root);

  CU_ASSERT(0 == rv);
  CU_ASSERT(a == root->first_child);
  CU_ASSERT(NULL == root->next_sibling);
  CU_ASSERT(1 == root->num_children);
  CU_ASSERT(nghttp3_pq_empty(&root->pq));
  CU_ASSERT(root == a->parent);
  CU_ASSERT(NULL == a->next_sibling);
  CU_ASSERT(0 == a->num_children);

  nghttp3_tnode_free(a);
  nghttp3_tnode_free(root);

  /* Insert a node to root which has descendants */
  nghttp3_tnode_init(root, &rnid, 0, NGHTTP3_DEFAULT_WEIGHT, NULL, mem);
  nghttp3_tnode_init(c, &snid, 0, 19, root, mem);
  nghttp3_tnode_init(b, &snid, 0, 99, root, mem);
  nghttp3_tnode_init(a, &snid, 0, 12, NULL, mem);

  nghttp3_tnode_insert(a, root);

  CU_ASSERT(0 == rv);
  CU_ASSERT(a == root->first_child);
  CU_ASSERT(3 == root->num_children);
  CU_ASSERT(nghttp3_pq_empty(&root->pq));
  CU_ASSERT(root == a->parent);
  CU_ASSERT(b == a->next_sibling);

  nghttp3_tnode_free(c);
  nghttp3_tnode_free(b);
  nghttp3_tnode_free(a);
  nghttp3_tnode_free(root);

  /* Remove a node from root */
  nghttp3_tnode_init(root, &rnid, 0, NGHTTP3_DEFAULT_WEIGHT, NULL, mem);
  nghttp3_tnode_init(a, &snid, 0, 1, root, mem);

  nghttp3_tnode_remove(a);

  CU_ASSERT(NULL == root->first_child);
  CU_ASSERT(NULL == root->next_sibling);
  CU_ASSERT(0 == root->num_children);
  CU_ASSERT(nghttp3_pq_empty(&root->pq));
  CU_ASSERT(NULL == a->parent);
  CU_ASSERT(NULL == a->next_sibling);
  CU_ASSERT(NULL == a->first_child);

  nghttp3_tnode_free(a);
  nghttp3_tnode_free(root);

  /* Remove a node with siblings from root */
  nghttp3_tnode_init(root, &rnid, 0, NGHTTP3_DEFAULT_WEIGHT, NULL, mem);
  nghttp3_tnode_init(c, &snid, 0, 250, root, mem);
  nghttp3_tnode_init(a, &snid, 0, 249, root, mem);
  nghttp3_tnode_init(b, &snid, 0, 112, root, mem);

  nghttp3_tnode_remove(a);

  CU_ASSERT(b == root->first_child);
  CU_ASSERT(2 == root->num_children);
  CU_ASSERT(NULL == a->parent);
  CU_ASSERT(NULL == a->next_sibling);
  CU_ASSERT(c == b->next_sibling);

  nghttp3_tnode_free(c);
  nghttp3_tnode_free(b);
  nghttp3_tnode_free(a);
  nghttp3_tnode_free(root);

  /* Remove a scheduled node from root */
  nghttp3_tnode_init(root, &rnid, 0, NGHTTP3_DEFAULT_WEIGHT, NULL, mem);
  nghttp3_tnode_init(a, &snid, 0, 1, root, mem);

  nghttp3_tnode_schedule(a, 1200);

  CU_ASSERT(1 == nghttp3_pq_size(&root->pq));

  nghttp3_tnode_remove(a);

  CU_ASSERT(NULL == root->first_child);
  CU_ASSERT(nghttp3_pq_empty(&root->pq));
  CU_ASSERT(!nghttp3_tnode_is_scheduled(a));

  nghttp3_tnode_free(a);
  nghttp3_tnode_free(root);

  /* Squash a node from root */
  nghttp3_tnode_init(root, &rnid, 0, NGHTTP3_DEFAULT_WEIGHT, NULL, mem);
  nghttp3_tnode_init(a, &snid, 0, 128, root, mem);
  nghttp3_tnode_init(c, &snid, 0, 5, a, mem);
  nghttp3_tnode_init(b, &snid, 0, 3, a, mem);

  nghttp3_tnode_squash(a);

  CU_ASSERT(b == root->first_child);
  CU_ASSERT(nghttp3_pq_empty(&root->pq));
  CU_ASSERT(root == b->parent);
  CU_ASSERT(3 * 128 / 2 == b->weight);
  CU_ASSERT(root == c->parent);
  CU_ASSERT(5 * 128 / 2 == c->weight);
  CU_ASSERT(NULL == a->parent);
  CU_ASSERT(NULL == a->next_sibling);
  CU_ASSERT(NULL == a->first_child);

  nghttp3_tnode_free(c);
  nghttp3_tnode_free(b);
  nghttp3_tnode_free(a);
  nghttp3_tnode_free(root);

  /* Squash a node with siblings from root */
  nghttp3_tnode_init(root, &rnid, 0, NGHTTP3_DEFAULT_WEIGHT, NULL, mem);
  nghttp3_tnode_init(e, &snid, 0, 128, root, mem);
  nghttp3_tnode_init(a, &snid, 0, 1, root, mem);
  nghttp3_tnode_init(b, &snid, 0, 129, root, mem);
  nghttp3_tnode_init(d, &snid, 0, 9, a, mem);
  nghttp3_tnode_init(c, &snid, 0, 12, a, mem);

  nghttp3_tnode_squash(a);

  CU_ASSERT(b == root->first_child);
  CU_ASSERT(c == b->next_sibling);
  CU_ASSERT(e == d->next_sibling);
  CU_ASSERT(root == c->parent);
  CU_ASSERT(root == d->parent);
  CU_ASSERT(NULL == a->parent);
  CU_ASSERT(NULL == a->next_sibling);
  CU_ASSERT(NULL == a->first_child);

  nghttp3_tnode_free(e);
  nghttp3_tnode_free(d);
  nghttp3_tnode_free(c);
  nghttp3_tnode_free(b);
  nghttp3_tnode_free(a);
  nghttp3_tnode_free(root);

  /* Squash a scheduled node with scheduled siblings from root */
  nghttp3_tnode_init(root, &rnid, 0, NGHTTP3_DEFAULT_WEIGHT, NULL, mem);
  nghttp3_tnode_init(e, &snid, 0, 128, root, mem);
  nghttp3_tnode_init(a, &snid, 0, 1, root, mem);
  nghttp3_tnode_init(b, &snid, 0, 129, root, mem);
  nghttp3_tnode_init(d, &snid, 0, 9, a, mem);
  nghttp3_tnode_init(c, &snid, 0, 12, a, mem);

  nghttp3_tnode_schedule(a, 1000);
  nghttp3_tnode_schedule(c, 100);
  nghttp3_tnode_schedule(d, 100);

  nghttp3_tnode_squash(a);

  CU_ASSERT(2 == nghttp3_pq_size(&root->pq));

  nghttp3_tnode_free(e);
  nghttp3_tnode_free(d);
  nghttp3_tnode_free(c);
  nghttp3_tnode_free(b);
  nghttp3_tnode_free(a);
  nghttp3_tnode_free(root);
}

void test_nghttp3_tnode_schedule(void) {
  const nghttp3_mem *mem = nghttp3_mem_default();
  nghttp3_tnode nodes[3];
  nghttp3_tnode *root = &nodes[0], *a = &nodes[1], *b = &nodes[2];
  nghttp3_node_id rnid, snid;
  int rv;

  nghttp3_node_id_init(&rnid, NGHTTP3_NODE_ID_TYPE_ROOT, 0);
  nghttp3_node_id_init(&snid, NGHTTP3_NODE_ID_TYPE_STREAM, 0);

  /* Unscheduled internal node should be scheduled */
  nghttp3_tnode_init(root, &rnid, 0, NGHTTP3_DEFAULT_WEIGHT, NULL, mem);
  nghttp3_tnode_init(b, &snid, 0, 100, root, mem);
  nghttp3_tnode_init(a, &snid, 0, 15, b, mem);

  rv = nghttp3_tnode_schedule(a, 0);

  CU_ASSERT(0 == rv);
  CU_ASSERT(nghttp3_tnode_is_scheduled(b));
  CU_ASSERT(nghttp3_tnode_is_scheduled(a));
  CU_ASSERT(0 == b->cycle);
  CU_ASSERT(0 == a->cycle);

  nghttp3_tnode_free(b);
  nghttp3_tnode_free(a);
  nghttp3_tnode_free(root);

  /* Scheduled internal node is updated if nwrite > 0 */
  nghttp3_tnode_init(root, &rnid, 0, NGHTTP3_DEFAULT_WEIGHT, NULL, mem);
  nghttp3_tnode_init(b, &snid, 0, 100, root, mem);
  nghttp3_tnode_init(a, &snid, 0, 15, b, mem);

  rv = nghttp3_tnode_schedule(b, 0);

  CU_ASSERT(0 == rv);
  CU_ASSERT(0 == b->cycle);

  rv = nghttp3_tnode_schedule(a, 1000);

  CU_ASSERT(0 == rv);
  CU_ASSERT(nghttp3_tnode_is_scheduled(b));
  CU_ASSERT(nghttp3_tnode_is_scheduled(a));
  CU_ASSERT(b->cycle > 0);

  nghttp3_tnode_free(b);
  nghttp3_tnode_free(a);
  nghttp3_tnode_free(root);

  /* Unschedule inactive internal node */
  nghttp3_tnode_init(root, &rnid, 0, NGHTTP3_DEFAULT_WEIGHT, NULL, mem);
  nghttp3_tnode_init(b, &snid, 0, 100, root, mem);
  nghttp3_tnode_init(a, &snid, 0, 15, b, mem);

  nghttp3_tnode_schedule(a, 0);

  CU_ASSERT(nghttp3_tnode_is_scheduled(b));
  CU_ASSERT(nghttp3_tnode_is_scheduled(a));

  nghttp3_tnode_unschedule(a);

  CU_ASSERT(!nghttp3_tnode_is_scheduled(b));
  CU_ASSERT(!nghttp3_tnode_is_scheduled(a));

  nghttp3_tnode_free(b);
  nghttp3_tnode_free(a);
  nghttp3_tnode_free(root);

  /* Active internal node remains scheduled */
  nghttp3_tnode_init(root, &rnid, 0, NGHTTP3_DEFAULT_WEIGHT, NULL, mem);
  nghttp3_tnode_init(b, &snid, 0, 100, root, mem);
  nghttp3_tnode_init(a, &snid, 0, 15, b, mem);

  nghttp3_tnode_schedule(b, 0);
  nghttp3_tnode_schedule(a, 0);

  CU_ASSERT(nghttp3_tnode_is_scheduled(b));
  CU_ASSERT(nghttp3_tnode_is_scheduled(a));

  nghttp3_tnode_unschedule(a);

  CU_ASSERT(nghttp3_tnode_is_scheduled(b));
  CU_ASSERT(!nghttp3_tnode_is_scheduled(a));

  nghttp3_tnode_free(b);
  nghttp3_tnode_free(a);
  nghttp3_tnode_free(root);
}
