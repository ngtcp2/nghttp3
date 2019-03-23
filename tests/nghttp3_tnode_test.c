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
  int rv;

  /* Insert a node to empty root */
  nghttp3_tnode_init(root, NGHTTP3_ID_TYPE_ROOT, 0, 0, NGHTTP3_DEFAULT_WEIGHT,
                     NULL, mem);
  nghttp3_tnode_init(a, NGHTTP3_ID_TYPE_STREAM, 0, 0, 12, NULL, mem);

  rv = nghttp3_tnode_insert(a, root, UINT64_MAX);

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
  nghttp3_tnode_init(root, NGHTTP3_ID_TYPE_ROOT, 0, 0, NGHTTP3_DEFAULT_WEIGHT,
                     NULL, mem);
  nghttp3_tnode_init(c, NGHTTP3_ID_TYPE_STREAM, 0, 0, 19, root, mem);
  nghttp3_tnode_init(b, NGHTTP3_ID_TYPE_STREAM, 0, 0, 99, root, mem);
  nghttp3_tnode_init(a, NGHTTP3_ID_TYPE_STREAM, 0, 0, 12, NULL, mem);

  rv = nghttp3_tnode_insert(a, root, UINT64_MAX);

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

  /* Insert a node with cycle to a root */
  nghttp3_tnode_init(root, NGHTTP3_ID_TYPE_ROOT, 0, 0, NGHTTP3_DEFAULT_WEIGHT,
                     NULL, mem);
  nghttp3_tnode_init(a, NGHTTP3_ID_TYPE_STREAM, 0, 0, 12, NULL, mem);

  rv = nghttp3_tnode_insert(a, root, 1033);

  CU_ASSERT(0 == rv);
  CU_ASSERT(1 == nghttp3_pq_size(&root->pq));
  CU_ASSERT(&a->pe == nghttp3_pq_top(&root->pq));
  CU_ASSERT(1033 == a->cycle);

  nghttp3_tnode_free(a);
  nghttp3_tnode_free(root);

  /* Insert a node with cycle to a root with non-empty pq */
  nghttp3_tnode_init(root, NGHTTP3_ID_TYPE_ROOT, 0, 0, NGHTTP3_DEFAULT_WEIGHT,
                     NULL, mem);
  nghttp3_tnode_init(b, NGHTTP3_ID_TYPE_STREAM, 0, 0, 99, root, mem);
  nghttp3_tnode_init(a, NGHTTP3_ID_TYPE_STREAM, 0, 0, 12, NULL, mem);

  nghttp3_tnode_schedule(b, 1000);

  rv = nghttp3_tnode_insert(a, root, 109);

  CU_ASSERT(0 == rv);
  CU_ASSERT(2 == nghttp3_pq_size(&root->pq));
  CU_ASSERT(&b->pe == nghttp3_pq_top(&root->pq));
  CU_ASSERT(b->cycle + 109 == a->cycle);

  nghttp3_tnode_free(b);
  nghttp3_tnode_free(a);
  nghttp3_tnode_free(root);

  /* Remove a node from root */
  nghttp3_tnode_init(root, NGHTTP3_ID_TYPE_ROOT, 0, 0, NGHTTP3_DEFAULT_WEIGHT,
                     NULL, mem);
  nghttp3_tnode_init(a, NGHTTP3_ID_TYPE_STREAM, 0, 0, 1, root, mem);

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
  nghttp3_tnode_init(root, NGHTTP3_ID_TYPE_ROOT, 0, 0, NGHTTP3_DEFAULT_WEIGHT,
                     NULL, mem);
  nghttp3_tnode_init(c, NGHTTP3_ID_TYPE_STREAM, 0, 0, 250, root, mem);
  nghttp3_tnode_init(a, NGHTTP3_ID_TYPE_STREAM, 0, 0, 249, root, mem);
  nghttp3_tnode_init(b, NGHTTP3_ID_TYPE_STREAM, 0, 0, 112, root, mem);

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
  nghttp3_tnode_init(root, NGHTTP3_ID_TYPE_ROOT, 0, 0, NGHTTP3_DEFAULT_WEIGHT,
                     NULL, mem);
  nghttp3_tnode_init(a, NGHTTP3_ID_TYPE_STREAM, 0, 0, 1, root, mem);

  nghttp3_tnode_schedule(a, 1200);

  CU_ASSERT(1 == nghttp3_pq_size(&root->pq));

  nghttp3_tnode_remove(a);

  CU_ASSERT(NULL == root->first_child);
  CU_ASSERT(nghttp3_pq_empty(&root->pq));
  CU_ASSERT(!nghttp3_tnode_is_scheduled(a));

  nghttp3_tnode_free(a);
  nghttp3_tnode_free(root);

  /* Squash a node from root */
  nghttp3_tnode_init(root, NGHTTP3_ID_TYPE_ROOT, 0, 0, NGHTTP3_DEFAULT_WEIGHT,
                     NULL, mem);
  nghttp3_tnode_init(a, NGHTTP3_ID_TYPE_STREAM, 0, 0, 128, root, mem);
  nghttp3_tnode_init(c, NGHTTP3_ID_TYPE_STREAM, 0, 0, 5, a, mem);
  nghttp3_tnode_init(b, NGHTTP3_ID_TYPE_STREAM, 0, 0, 3, a, mem);

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
  nghttp3_tnode_init(root, NGHTTP3_ID_TYPE_ROOT, 0, 0, NGHTTP3_DEFAULT_WEIGHT,
                     NULL, mem);
  nghttp3_tnode_init(e, NGHTTP3_ID_TYPE_STREAM, 0, 0, 128, root, mem);
  nghttp3_tnode_init(a, NGHTTP3_ID_TYPE_STREAM, 0, 0, 1, root, mem);
  nghttp3_tnode_init(b, NGHTTP3_ID_TYPE_STREAM, 0, 0, 129, root, mem);
  nghttp3_tnode_init(d, NGHTTP3_ID_TYPE_STREAM, 0, 0, 9, a, mem);
  nghttp3_tnode_init(c, NGHTTP3_ID_TYPE_STREAM, 0, 0, 12, a, mem);

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
  nghttp3_tnode_init(root, NGHTTP3_ID_TYPE_ROOT, 0, 0, NGHTTP3_DEFAULT_WEIGHT,
                     NULL, mem);
  nghttp3_tnode_init(e, NGHTTP3_ID_TYPE_STREAM, 0, 0, 128, root, mem);
  nghttp3_tnode_init(a, NGHTTP3_ID_TYPE_STREAM, 0, 0, 1, root, mem);
  nghttp3_tnode_init(b, NGHTTP3_ID_TYPE_STREAM, 0, 0, 129, root, mem);
  nghttp3_tnode_init(d, NGHTTP3_ID_TYPE_STREAM, 0, 0, 9, a, mem);
  nghttp3_tnode_init(c, NGHTTP3_ID_TYPE_STREAM, 0, 0, 12, a, mem);

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
