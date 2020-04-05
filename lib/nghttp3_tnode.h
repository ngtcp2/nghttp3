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
#ifndef NGHTTP3_TNODE_H
#define NGHTTP3_TNODE_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <nghttp3/nghttp3.h>

#include "nghttp3_pq.h"

#define NGHTTP3_URGENCY_LEVELS 8
#define NGHTTP3_DEFAULT_URGENCY 1
#define NGHTTP3_TNODE_MAX_CYCLE_GAP ((1llu << 24) * 256 + 255)

typedef enum {
  NGHTTP3_NODE_ID_TYPE_STREAM = 0x00,
  NGHTTP3_NODE_ID_TYPE_PUSH = 0x01,
  NGHTTP3_NODE_ID_TYPE_PLACEHOLDER = 0x02,
  NGHTTP3_NODE_ID_TYPE_ROOT = 0x03,
  /* NGHTTP3_NODE_ID_TYPE_UT is defined for unit test */
  NGHTTP3_NODE_ID_TYPE_UT = 0xff,
} nghttp3_node_id_type;

typedef struct {
  nghttp3_node_id_type type;
  int64_t id;
} nghttp3_node_id;

nghttp3_node_id *nghttp3_node_id_init(nghttp3_node_id *nid,
                                      nghttp3_node_id_type type, int64_t id);

int nghttp3_node_id_eq(const nghttp3_node_id *a, const nghttp3_node_id *b);

struct nghttp3_tnode;
typedef struct nghttp3_tnode nghttp3_tnode;

struct nghttp3_tnode {
  nghttp3_pq_entry pe;
  size_t num_children;
  nghttp3_node_id nid;
  uint64_t seq;
  uint64_t cycle;
  uint32_t urgency;
  int inc;
  /* active is defined for unit test and is nonzero if this node is
     active. */
  int active;
};

void nghttp3_tnode_init(nghttp3_tnode *tnode, const nghttp3_node_id *nid,
                        uint64_t seq, uint32_t urgency, int inc);

void nghttp3_tnode_free(nghttp3_tnode *tnode);

/*
 * nghttp3_tnode_is_active returns nonzero if |tnode| is active.  Only
 * NGHTTP3_NODE_ID_TYPE_STREAM and NGHTTP3_NODE_ID_TYPE_PUSH (and
 * NGHTTP3_NODE_ID_TYPE_UT for unit test) can become active.
 */
int nghttp3_tnode_is_active(nghttp3_tnode *tnode);

void nghttp3_tnode_unschedule(nghttp3_tnode *tnode, nghttp3_pq *pq);

/*
 * nghttp3_tnode_schedule schedules |tnode| using |nwrite| as penalty.
 * If |tnode| has already been scheduled, it is rescheduled by the
 * amount of |nwrite|.
 */
int nghttp3_tnode_schedule(nghttp3_tnode *tnode, nghttp3_pq *pq, size_t nwrite);

/*
 * nghttp3_tnode_is_scheduled returns nonzero if |tnode| is scheduled.
 */
int nghttp3_tnode_is_scheduled(nghttp3_tnode *tnode);

#endif /* NGHTTP3_TNODE_H */
