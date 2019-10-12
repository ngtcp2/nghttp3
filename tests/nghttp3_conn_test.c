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
#include "nghttp3_conn_test.h"

#include <assert.h>

#include <CUnit/CUnit.h>

#include "nghttp3_conn.h"
#include "nghttp3_macro.h"
#include "nghttp3_conv.h"
#include "nghttp3_frame.h"
#include "nghttp3_vec.h"
#include "nghttp3_test_helper.h"
#include "nghttp3_http.h"

static uint8_t nulldata[4096];

typedef struct {
  struct {
    size_t nblock;
    size_t left;
    size_t step;
  } data;
  struct {
    size_t acc;
  } ack;
  struct {
    size_t ncalled;
    int64_t push_id;
  } cancel_push_cb;
  struct {
    size_t ncalled;
  } recv_data_cb;
  struct {
    size_t ncalled;
  } push_stream_cb;
} userdata;

static int acked_stream_data(nghttp3_conn *conn, int64_t stream_id,
                             size_t datalen, void *user_data,
                             void *stream_user_data) {
  userdata *ud = user_data;

  (void)conn;
  (void)stream_id;
  (void)stream_user_data;

  ud->ack.acc += datalen;

  return 0;
}

static int begin_headers(nghttp3_conn *conn, int64_t stream_id, void *user_data,
                         void *stream_user_data) {
  (void)conn;
  (void)stream_id;
  (void)stream_user_data;
  (void)user_data;
  return 0;
}

static int recv_header(nghttp3_conn *conn, int64_t stream_id, int32_t token,
                       nghttp3_rcbuf *name, nghttp3_rcbuf *value, uint8_t flags,
                       void *user_data, void *stream_user_data) {
  (void)conn;
  (void)stream_id;
  (void)token;
  (void)name;
  (void)value;
  (void)flags;
  (void)stream_user_data;
  (void)user_data;
  return 0;
}

static int end_headers(nghttp3_conn *conn, int64_t stream_id, void *user_data,
                       void *stream_user_data) {
  (void)conn;
  (void)stream_id;
  (void)stream_user_data;
  (void)user_data;
  return 0;
}

static ssize_t step_read_data(nghttp3_conn *conn, int64_t stream_id,
                              nghttp3_vec *vec, size_t veccnt, uint32_t *pflags,
                              void *user_data, void *stream_user_data) {
  userdata *ud = user_data;
  size_t n = nghttp3_min(ud->data.left, ud->data.step);

  (void)conn;
  (void)stream_id;
  (void)veccnt;
  (void)stream_user_data;

  ud->data.left -= n;
  if (ud->data.left == 0) {
    *pflags = NGHTTP3_DATA_FLAG_EOF;

    if (n == 0) {
      return 0;
    }
  }

  vec[0].base = nulldata;
  vec[0].len = n;

  return 1;
}

static ssize_t block_then_step_read_data(nghttp3_conn *conn, int64_t stream_id,
                                         nghttp3_vec *vec, size_t veccnt,
                                         uint32_t *pflags, void *user_data,
                                         void *stream_user_data) {
  userdata *ud = user_data;

  if (ud->data.nblock == 0) {
    return step_read_data(conn, stream_id, vec, veccnt, pflags, user_data,
                          stream_user_data);
  }

  --ud->data.nblock;

  return NGHTTP3_ERR_WOULDBLOCK;
}

static ssize_t step_then_block_read_data(nghttp3_conn *conn, int64_t stream_id,
                                         nghttp3_vec *vec, size_t veccnt,
                                         uint32_t *pflags, void *user_data,
                                         void *stream_user_data) {
  ssize_t rv;

  rv = step_read_data(conn, stream_id, vec, veccnt, pflags, user_data,
                      stream_user_data);

  assert(rv >= 0);

  if (*pflags &  NGHTTP3_DATA_FLAG_EOF) {
    *pflags &= (uint32_t)~NGHTTP3_DATA_FLAG_EOF;

    if (nghttp3_vec_len(vec, (size_t)rv) == 0) {
      return NGHTTP3_ERR_WOULDBLOCK;
    }
  }

  return rv;
}

static int cancel_push(nghttp3_conn *conn, int64_t push_id, int64_t stream_id,
                       void *user_data, void *stream_user_data) {
  userdata *ud = user_data;

  (void)conn;
  (void)stream_id;
  (void)stream_user_data;

  ++ud->cancel_push_cb.ncalled;
  ud->cancel_push_cb.push_id = push_id;

  return 0;
}

static int recv_data(nghttp3_conn *conn, int64_t stream_id, const uint8_t *data,
                     size_t datalen, void *user_data, void *stream_user_data) {
  userdata *ud = user_data;

  (void)conn;
  (void)stream_id;
  (void)data;
  (void)datalen;
  (void)stream_user_data;

  ++ud->recv_data_cb.ncalled;

  return 0;
}

static int push_stream(nghttp3_conn *conn, int64_t push_id, int64_t stream_id,
                       void *user_data) {
  userdata *ud = user_data;

  (void)conn;
  (void)push_id;
  (void)stream_id;

  ++ud->push_stream_cb.ncalled;

  return 0;
}

void test_nghttp3_conn_read_control(void) {
  const nghttp3_mem *mem = nghttp3_mem_default();
  nghttp3_conn *conn;
  nghttp3_conn_callbacks callbacks;
  nghttp3_conn_settings settings;
  int rv;
  uint8_t rawbuf[2048];
  nghttp3_buf buf;
  struct {
    nghttp3_frame_settings settings;
    nghttp3_settings_entry iv[15];
  } fr;
  ssize_t nconsumed;
  nghttp3_settings_entry *iv;
  size_t i;

  memset(&callbacks, 0, sizeof(callbacks));
  nghttp3_conn_settings_default(&settings);
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  fr.settings.hd.type = NGHTTP3_FRAME_SETTINGS;
  iv = fr.settings.iv;
  iv[0].id = NGHTTP3_SETTINGS_ID_MAX_HEADER_LIST_SIZE;
  iv[0].value = 65536;
  iv[1].id = 1000000009;
  iv[1].value = 1000000007;
  iv[2].id = NGHTTP3_SETTINGS_ID_QPACK_MAX_TABLE_CAPACITY;
  iv[2].value = 4096;
  iv[3].id = NGHTTP3_SETTINGS_ID_QPACK_BLOCKED_STREAMS;
  iv[3].value = 99;
  fr.settings.niv = 4;

  nghttp3_write_frame(&buf, (nghttp3_frame *)&fr);

  rv = nghttp3_conn_server_new(&conn, &callbacks, &settings, mem, NULL);

  CU_ASSERT(0 == rv);

  nconsumed = nghttp3_conn_read_stream(conn, 2, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT(nconsumed == (ssize_t)nghttp3_buf_len(&buf));
  CU_ASSERT(65536 == conn->remote.settings.max_header_list_size);
  CU_ASSERT(4096 == conn->remote.settings.qpack_max_table_capacity);
  CU_ASSERT(99 == conn->remote.settings.qpack_blocked_streams);

  nghttp3_conn_del(conn);

  /* Feed 1 byte at a time to verify that state machine works */
  rv = nghttp3_conn_server_new(&conn, &callbacks, &settings, mem, NULL);

  CU_ASSERT(0 == rv);

  for (i = 0; i < nghttp3_buf_len(&buf); ++i) {
    nconsumed =
        nghttp3_conn_read_stream(conn, 2, buf.pos + i, 1, /* fin = */ 0);

    CU_ASSERT(1 == nconsumed);
  }

  CU_ASSERT(65536 == conn->remote.settings.max_header_list_size);
  CU_ASSERT(4096 == conn->remote.settings.qpack_max_table_capacity);
  CU_ASSERT(99 == conn->remote.settings.qpack_blocked_streams);

  nghttp3_conn_del(conn);
}

void test_nghttp3_conn_write_control(void) {
  const nghttp3_mem *mem = nghttp3_mem_default();
  nghttp3_conn *conn;
  nghttp3_conn_callbacks callbacks;
  nghttp3_conn_settings settings;
  nghttp3_vec vec[256];
  ssize_t sveccnt;
  int rv;
  int64_t stream_id;
  int fin;

  memset(&callbacks, 0, sizeof(callbacks));
  nghttp3_conn_settings_default(&settings);

  rv = nghttp3_conn_server_new(&conn, &callbacks, &settings, mem, NULL);

  CU_ASSERT(0 == rv);

  rv = nghttp3_conn_bind_control_stream(conn, 3);

  CU_ASSERT(0 == rv);
  CU_ASSERT(NULL != conn->tx.ctrl);
  CU_ASSERT(NGHTTP3_STREAM_TYPE_CONTROL == conn->tx.ctrl->type);

  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  CU_ASSERT(3 == stream_id);
  CU_ASSERT(1 == sveccnt);
  CU_ASSERT(vec[0].len > 1);
  CU_ASSERT(NGHTTP3_STREAM_TYPE_CONTROL == vec[0].base[0]);

  nghttp3_conn_del(conn);
}

void test_nghttp3_conn_submit_request(void) {
  const nghttp3_mem *mem = nghttp3_mem_default();
  nghttp3_conn *conn;
  nghttp3_conn_callbacks callbacks;
  nghttp3_conn_settings settings;
  nghttp3_vec vec[256];
  ssize_t sveccnt;
  int rv;
  int64_t stream_id;
  const nghttp3_nv nva[] = {
      MAKE_NV(":path", "/"),
      MAKE_NV(":authority", "example.com"),
      MAKE_NV(":scheme", "https"),
      MAKE_NV(":method", "GET"),
  };
  size_t len;
  size_t i;
  nghttp3_stream *stream;
  userdata ud;
  nghttp3_data_reader dr;
  int fin;

  memset(&callbacks, 0, sizeof(callbacks));
  memset(&ud, 0, sizeof(ud));
  nghttp3_conn_settings_default(&settings);

  callbacks.acked_stream_data = acked_stream_data;

  ud.data.left = 2000;
  ud.data.step = 1200;

  rv = nghttp3_conn_client_new(&conn, &callbacks, &settings, mem, &ud);

  CU_ASSERT(0 == rv);

  rv = nghttp3_conn_bind_qpack_streams(conn, 6, 10);

  CU_ASSERT(0 == rv);
  CU_ASSERT(NULL != conn->tx.qenc);
  CU_ASSERT(NGHTTP3_STREAM_TYPE_QPACK_ENCODER == conn->tx.qenc->type);
  CU_ASSERT(NULL != conn->tx.qdec);
  CU_ASSERT(NGHTTP3_STREAM_TYPE_QPACK_DECODER == conn->tx.qdec->type);

  dr.read_data = step_read_data;
  rv = nghttp3_conn_submit_request(conn, 0, nva, nghttp3_arraylen(nva), &dr,
                                   NULL);

  CU_ASSERT(0 == rv);

  /* This will write QPACK decoder stream; just stream type */
  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  CU_ASSERT(10 == stream_id);
  CU_ASSERT(1 == sveccnt);
  CU_ASSERT(1 == nghttp3_ringbuf_len(&conn->tx.qdec->outq));
  CU_ASSERT(0 == conn->tx.qdec->outq_idx);
  CU_ASSERT(0 == conn->tx.qdec->outq_offset);

  /* Calling twice will return the same result */
  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  CU_ASSERT(10 == stream_id);
  CU_ASSERT(1 == sveccnt);

  rv = nghttp3_conn_add_write_offset(conn, 10, vec[0].len);

  CU_ASSERT(0 == rv);
  CU_ASSERT(1 == nghttp3_ringbuf_len(&conn->tx.qdec->outq));
  CU_ASSERT(1 == conn->tx.qdec->outq_idx);
  CU_ASSERT(0 == conn->tx.qdec->outq_offset);

  rv = nghttp3_conn_add_ack_offset(conn, 10, vec[0].len);

  CU_ASSERT(0 == rv);
  CU_ASSERT(0 == nghttp3_ringbuf_len(&conn->tx.qdec->outq));
  CU_ASSERT(0 == conn->tx.qdec->outq_idx);
  CU_ASSERT(0 == conn->tx.qdec->outq_offset);
  CU_ASSERT(0 == conn->tx.qdec->ack_offset);

  /* This will write QPACK encoder stream; just stream type */
  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  CU_ASSERT(6 == stream_id);
  CU_ASSERT(1 == sveccnt);

  rv = nghttp3_conn_add_write_offset(conn, 6, vec[0].len);

  CU_ASSERT(0 == rv);

  rv = nghttp3_conn_add_ack_offset(conn, 6, vec[0].len);

  CU_ASSERT(0 == rv);

  /* This will write request stream */
  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  CU_ASSERT(0 == stream_id);
  CU_ASSERT(2 == sveccnt);

  len = nghttp3_vec_len(vec, (size_t)sveccnt);
  for (i = 0; i < len; ++i) {
    rv = nghttp3_conn_add_write_offset(conn, 0, 1);

    CU_ASSERT(0 == rv);

    rv = nghttp3_conn_add_ack_offset(conn, 0, 1);

    CU_ASSERT(0 == rv);
  }

  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  CU_ASSERT(0 == stream_id);
  CU_ASSERT(2 == sveccnt);

  len = nghttp3_vec_len(vec, (size_t)sveccnt);

  for (i = 0; i < len; ++i) {
    rv = nghttp3_conn_add_write_offset(conn, 0, 1);

    CU_ASSERT(0 == rv);

    rv = nghttp3_conn_add_ack_offset(conn, 0, 1);

    CU_ASSERT(0 == rv);
  }

  stream = nghttp3_conn_find_stream(conn, 0);

  CU_ASSERT(0 == nghttp3_ringbuf_len(&stream->outq));
  CU_ASSERT(0 == nghttp3_ringbuf_len(&stream->chunks));
  CU_ASSERT(0 == stream->outq_idx);
  CU_ASSERT(0 == stream->outq_offset);
  CU_ASSERT(0 == stream->ack_offset);
  CU_ASSERT(2000 == ud.ack.acc);

  nghttp3_conn_del(conn);
}

void test_nghttp3_conn_submit_push_promise(void) {
  const nghttp3_mem *mem = nghttp3_mem_default();
  nghttp3_conn *conn;
  nghttp3_conn_callbacks callbacks;
  nghttp3_conn_settings settings;
  const nghttp3_nv nva[] = {
      MAKE_NV(":path", "/"),
      MAKE_NV(":authority", "example.com"),
      MAKE_NV(":scheme", "https"),
      MAKE_NV(":method", "GET"),
  };
  nghttp3_stream *stream;
  nghttp3_push_promise *pp;
  int64_t push_id;
  int rv;
  nghttp3_vec vec[256];
  int fin;
  int64_t stream_id;
  ssize_t sveccnt;

  memset(&callbacks, 0, sizeof(callbacks));
  nghttp3_conn_settings_default(&settings);

  nghttp3_conn_server_new(&conn, &callbacks, &settings, mem, NULL);
  conn->local.uni.max_pushes = 1;
  conn->remote.bidi.max_client_streams = 1;
  nghttp3_conn_bind_qpack_streams(conn, 7, 11);

  nghttp3_conn_create_stream(conn, &stream, 0);

  rv = nghttp3_conn_submit_push_promise(conn, &push_id, 0, nva,
                                        nghttp3_arraylen(nva));

  CU_ASSERT(0 == rv);
  CU_ASSERT(0 == push_id);

  for (;;) {
    sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                         nghttp3_arraylen(vec));

    CU_ASSERT(sveccnt >= 0);

    if (sveccnt <= 0) {
      break;
    }

    rv = nghttp3_conn_add_write_offset(conn, stream_id,
                                       nghttp3_vec_len(vec, (size_t)sveccnt));

    CU_ASSERT(0 == rv);
  }

  rv = nghttp3_conn_bind_push_stream(conn, push_id, 15);

  CU_ASSERT(0 == rv);

  pp = nghttp3_conn_find_push_promise(conn, push_id);
  stream = nghttp3_conn_find_stream(conn, 15);

  CU_ASSERT(stream == pp->stream);
  CU_ASSERT(pp == stream->pp);
  CU_ASSERT(NULL == stream->node.parent);
  CU_ASSERT(NULL == stream->node.first_child);
  CU_ASSERT(NULL == stream->node.next_sibling);

  nghttp3_conn_del(conn);
}

void test_nghttp3_conn_http_request(void) {
  const nghttp3_mem *mem = nghttp3_mem_default();
  nghttp3_conn *cl, *sv;
  nghttp3_conn_callbacks callbacks;
  nghttp3_conn_settings settings;
  nghttp3_vec vec[256];
  ssize_t sveccnt;
  ssize_t sconsumed;
  int rv;
  int64_t stream_id;
  const nghttp3_nv reqnva[] = {
      MAKE_NV(":path", "/"),
      MAKE_NV(":authority", "example.com"),
      MAKE_NV(":scheme", "https"),
      MAKE_NV(":method", "GET"),
  };
  const nghttp3_nv respnva[] = {
      MAKE_NV(":status", "200"),
      MAKE_NV("server", "nghttp3"),
      MAKE_NV("content-length", "1999"),
  };
  nghttp3_data_reader dr;
  int fin;
  userdata clud, svud;
  size_t i;

  memset(&callbacks, 0, sizeof(callbacks));
  nghttp3_conn_settings_default(&settings);
  memset(&clud, 0, sizeof(clud));

  callbacks.begin_headers = begin_headers;
  callbacks.recv_header = recv_header;
  callbacks.end_headers = end_headers;

  settings.qpack_max_table_capacity = 4096;
  settings.qpack_blocked_streams = 100;

  clud.data.left = 2000;
  clud.data.step = 1200;

  svud.data.left = 1999;
  svud.data.step = 1000;

  nghttp3_conn_client_new(&cl, &callbacks, &settings, mem, &clud);
  nghttp3_conn_server_new(&sv, &callbacks, &settings, mem, &svud);

  nghttp3_conn_bind_control_stream(cl, 2);
  nghttp3_conn_bind_control_stream(sv, 3);

  nghttp3_conn_bind_qpack_streams(cl, 6, 10);
  nghttp3_conn_bind_qpack_streams(sv, 7, 11);

  dr.read_data = step_read_data;
  rv = nghttp3_conn_submit_request(cl, 0, reqnva, nghttp3_arraylen(reqnva), &dr,
                                   NULL);

  CU_ASSERT(0 == rv);

  for (;;) {
    sveccnt = nghttp3_conn_writev_stream(cl, &stream_id, &fin, vec,
                                         nghttp3_arraylen(vec));

    CU_ASSERT(sveccnt >= 0);

    if (sveccnt <= 0) {
      break;
    }

    rv = nghttp3_conn_add_write_offset(cl, stream_id,
                                       nghttp3_vec_len(vec, (size_t)sveccnt));

    CU_ASSERT(0 == rv);

    for (i = 0; i < (size_t)sveccnt; ++i) {
      sconsumed =
          nghttp3_conn_read_stream(sv, stream_id, vec[i].base, vec[i].len,
                                   fin && i == (size_t)sveccnt - 1);
      CU_ASSERT(sconsumed >= 0);
    }

    rv = nghttp3_conn_add_ack_offset(cl, stream_id,
                                     nghttp3_vec_len(vec, (size_t)sveccnt));

    CU_ASSERT(0 == rv);
  }

  rv = nghttp3_conn_submit_response(sv, 0, respnva, nghttp3_arraylen(respnva),
                                    &dr);

  CU_ASSERT(0 == rv);

  for (;;) {
    sveccnt = nghttp3_conn_writev_stream(sv, &stream_id, &fin, vec,
                                         nghttp3_arraylen(vec));

    CU_ASSERT(sveccnt >= 0);

    if (sveccnt <= 0) {
      break;
    }

    rv = nghttp3_conn_add_write_offset(sv, stream_id,
                                       nghttp3_vec_len(vec, (size_t)sveccnt));

    CU_ASSERT(0 == rv);

    for (i = 0; i < (size_t)sveccnt; ++i) {
      sconsumed =
          nghttp3_conn_read_stream(cl, stream_id, vec[i].base, vec[i].len,
                                   fin && i == (size_t)sveccnt - 1);
      CU_ASSERT(sconsumed >= 0);
    }

    rv = nghttp3_conn_add_ack_offset(sv, stream_id,
                                     nghttp3_vec_len(vec, (size_t)sveccnt));

    CU_ASSERT(0 == rv);
  }

  nghttp3_conn_del(sv);
  nghttp3_conn_del(cl);
}

static void check_http_header(const nghttp3_nv *nva, size_t nvlen, int request,
                              int want_lib_error) {
  uint8_t rawbuf[4096];
  nghttp3_buf buf;
  nghttp3_frame_headers fr;
  nghttp3_conn *conn;
  nghttp3_conn_callbacks callbacks;
  nghttp3_conn_settings settings;
  const nghttp3_mem *mem = nghttp3_mem_default();
  ssize_t sconsumed;
  nghttp3_qpack_encoder qenc;
  nghttp3_stream *stream;

  memset(&callbacks, 0, sizeof(callbacks));
  nghttp3_conn_settings_default(&settings);

  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)nva;
  fr.nvlen = nvlen;

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  if (request) {
    nghttp3_conn_server_new(&conn, &callbacks, &settings, mem, NULL);
    nghttp3_conn_set_max_client_streams_bidi(conn, 1);
  } else {
    nghttp3_conn_client_new(&conn, &callbacks, &settings, mem, NULL);
    nghttp3_conn_create_stream(conn, &stream, 0);
    stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;
  }

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  if (want_lib_error) {
    CU_ASSERT(want_lib_error == sconsumed);
  } else {
    CU_ASSERT(sconsumed > 0);
  }

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);
}

static void check_http_resp_header(const nghttp3_nv *nva, size_t nvlen,
                                   int want_lib_error) {
  check_http_header(nva, nvlen, /* request = */ 0, want_lib_error);
}

static void check_http_req_header(const nghttp3_nv *nva, size_t nvlen,
                                  int want_lib_error) {
  check_http_header(nva, nvlen, /* request = */ 1, want_lib_error);
}

void test_nghttp3_conn_http_resp_header(void) {
  /* test case for response */
  /* response header lacks :status */
  const nghttp3_nv nostatus_resnv[] = {
      MAKE_NV("server", "foo"),
  };
  /* response header has 2 :status */
  const nghttp3_nv dupstatus_resnv[] = {
      MAKE_NV(":status", "200"),
      MAKE_NV(":status", "200"),
  };
  /* response header has bad pseudo header :scheme */
  const nghttp3_nv badpseudo_resnv[] = {
      MAKE_NV(":status", "200"),
      MAKE_NV(":scheme", "https"),
  };
  /* response header has :status after regular header field */
  const nghttp3_nv latepseudo_resnv[] = {
      MAKE_NV("server", "foo"),
      MAKE_NV(":status", "200"),
  };
  /* response header has bad status code */
  const nghttp3_nv badstatus_resnv[] = {
      MAKE_NV(":status", "2000"),
  };
  /* response header has bad content-length */
  const nghttp3_nv badcl_resnv[] = {
      MAKE_NV(":status", "200"),
      MAKE_NV("content-length", "-1"),
  };
  /* response header has multiple content-length */
  const nghttp3_nv dupcl_resnv[] = {
      MAKE_NV(":status", "200"),
      MAKE_NV("content-length", "0"),
      MAKE_NV("content-length", "0"),
  };
  /* response header has disallowed header field */
  const nghttp3_nv badhd_resnv[] = {
      MAKE_NV(":status", "200"),
      MAKE_NV("connection", "close"),
  };
  /* response header has content-length with 100 status code */
  const nghttp3_nv cl1xx_resnv[] = {
      MAKE_NV(":status", "100"),
      MAKE_NV("content-length", "0"),
  };
  /* response header has 0 content-length with 204 status code */
  const nghttp3_nv cl204_resnv[] = {
      MAKE_NV(":status", "204"),
      MAKE_NV("content-length", "0"),
  };
  /* response header has nonzero content-length with 204 status
     code */
  const nghttp3_nv clnonzero204_resnv[] = {
      MAKE_NV(":status", "204"),
      MAKE_NV("content-length", "100"),
  };
  /* status code 101 should not be used in HTTP/3 because it is used
     for HTTP Upgrade which HTTP/3 removes. */
  const nghttp3_nv status101_resnv[] = {
      MAKE_NV(":status", "101"),
  };

  check_http_resp_header(nostatus_resnv, nghttp3_arraylen(nostatus_resnv),
                         NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
  check_http_resp_header(dupstatus_resnv, nghttp3_arraylen(dupstatus_resnv),
                         NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
  check_http_resp_header(badpseudo_resnv, nghttp3_arraylen(badpseudo_resnv),
                         NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
  check_http_resp_header(latepseudo_resnv, nghttp3_arraylen(latepseudo_resnv),
                         NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
  check_http_resp_header(badstatus_resnv, nghttp3_arraylen(badstatus_resnv),
                         NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
  check_http_resp_header(badcl_resnv, nghttp3_arraylen(badcl_resnv),
                         NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
  check_http_resp_header(dupcl_resnv, nghttp3_arraylen(dupcl_resnv),
                         NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
  check_http_resp_header(badhd_resnv, nghttp3_arraylen(badhd_resnv),
                         NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
  check_http_resp_header(cl1xx_resnv, nghttp3_arraylen(cl1xx_resnv),
                         NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
  /* This is allowed to work with widely used services. */
  check_http_resp_header(cl204_resnv, nghttp3_arraylen(cl204_resnv), 0);
  check_http_resp_header(clnonzero204_resnv,
                         nghttp3_arraylen(clnonzero204_resnv),
                         NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
  check_http_resp_header(status101_resnv, nghttp3_arraylen(status101_resnv),
                         NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
}

void test_nghttp3_conn_http_req_header(void) {
  /* test case for request */
  /* request header has no :path */
  const nghttp3_nv nopath_reqnv[] = {
      MAKE_NV(":scheme", "https"),
      MAKE_NV(":method", "GET"),
      MAKE_NV(":authority", "localhost"),
  };
  /* request header has CONNECT method, but followed by :path */
  const nghttp3_nv earlyconnect_reqnv[] = {
      MAKE_NV(":method", "CONNECT"),
      MAKE_NV(":scheme", "https"),
      MAKE_NV(":path", "/"),
      MAKE_NV(":authority", "localhost"),
  };
  /* request header has CONNECT method following :path */
  const nghttp3_nv lateconnect_reqnv[] = {
      MAKE_NV(":scheme", "https"),
      MAKE_NV(":path", "/"),
      MAKE_NV(":method", "CONNECT"),
      MAKE_NV(":authority", "localhost"),
  };
  /* request header has multiple :path */
  const nghttp3_nv duppath_reqnv[] = {
      MAKE_NV(":scheme", "https"),
      MAKE_NV(":method", "GET"),
      MAKE_NV(":authority", "localhost"),
      MAKE_NV(":path", "/"),
      MAKE_NV(":path", "/"),
  };
  /* request header has bad content-length */
  const nghttp3_nv badcl_reqnv[] = {
      MAKE_NV(":scheme", "https"),        MAKE_NV(":method", "POST"),
      MAKE_NV(":authority", "localhost"), MAKE_NV(":path", "/"),
      MAKE_NV("content-length", "-1"),
  };
  /* request header has multiple content-length */
  const nghttp3_nv dupcl_reqnv[] = {
      MAKE_NV(":scheme", "https"),        MAKE_NV(":method", "POST"),
      MAKE_NV(":authority", "localhost"), MAKE_NV(":path", "/"),
      MAKE_NV("content-length", "0"),     MAKE_NV("content-length", "0"),
  };
  /* request header has disallowed header field */
  const nghttp3_nv badhd_reqnv[] = {
      MAKE_NV(":scheme", "https"),        MAKE_NV(":method", "GET"),
      MAKE_NV(":authority", "localhost"), MAKE_NV(":path", "/"),
      MAKE_NV("connection", "close"),
  };
  /* request header has :authority header field containing illegal
     characters */
  const nghttp3_nv badauthority_reqnv[] = {
      MAKE_NV(":scheme", "https"),
      MAKE_NV(":method", "GET"),
      MAKE_NV(":authority", "\x0d\x0alocalhost"),
      MAKE_NV(":path", "/"),
  };
  /* request header has regular header field containing illegal
     character before all mandatory header fields are seen. */
  const nghttp3_nv badhdbtw_reqnv[] = {
      MAKE_NV(":scheme", "https"), MAKE_NV(":method", "GET"),
      MAKE_NV("foo", "\x0d\x0a"),  MAKE_NV(":authority", "localhost"),
      MAKE_NV(":path", "/"),
  };
  /* request header has "*" in :path header field while method is GET.
     :path is received before :method */
  const nghttp3_nv asteriskget1_reqnv[] = {
      MAKE_NV(":path", "*"),
      MAKE_NV(":scheme", "https"),
      MAKE_NV(":authority", "localhost"),
      MAKE_NV(":method", "GET"),
  };
  /* request header has "*" in :path header field while method is GET.
     :method is received before :path */
  const nghttp3_nv asteriskget2_reqnv[] = {
      MAKE_NV(":scheme", "https"),
      MAKE_NV(":authority", "localhost"),
      MAKE_NV(":method", "GET"),
      MAKE_NV(":path", "*"),
  };
  /* OPTIONS method can include "*" in :path header field.  :path is
     received before :method. */
  const nghttp3_nv asteriskoptions1_reqnv[] = {
      MAKE_NV(":path", "*"),
      MAKE_NV(":scheme", "https"),
      MAKE_NV(":authority", "localhost"),
      MAKE_NV(":method", "OPTIONS"),
  };
  /* OPTIONS method can include "*" in :path header field.  :method is
     received before :path. */
  const nghttp3_nv asteriskoptions2_reqnv[] = {
      MAKE_NV(":scheme", "https"),
      MAKE_NV(":authority", "localhost"),
      MAKE_NV(":method", "OPTIONS"),
      MAKE_NV(":path", "*"),
  };
  /* header name contains invalid character */
  const nghttp3_nv invalidname_reqnv[] = {
      MAKE_NV(":scheme", "https"),        MAKE_NV(":method", "GET"),
      MAKE_NV(":authority", "localhost"), MAKE_NV(":path", "/"),
      MAKE_NV("\x0foo", "zzz"),
  };
  /* header value contains invalid character */
  const nghttp3_nv invalidvalue_reqnv[] = {
      MAKE_NV(":scheme", "https"),        MAKE_NV(":method", "GET"),
      MAKE_NV(":authority", "localhost"), MAKE_NV(":path", "/"),
      MAKE_NV("foo", "\x0zzz"),
  };
  /* :protocol is not allowed unless it is enabled by the local
     endpoint. */
  /* :protocol is allowed if SETTINGS_CONNECT_PROTOCOL is enabled by
     the local endpoint. */
  /* SETTINGS_CONNECT_PROTOCOL has not been implemented yet. */
  const nghttp3_nv connectproto_reqnv[] = {
      MAKE_NV(":scheme", "https"),       MAKE_NV(":path", "/"),
      MAKE_NV(":method", "CONNECT"),     MAKE_NV(":authority", "localhost"),
      MAKE_NV(":protocol", "websocket"),
  };
  /* :protocol is only allowed with CONNECT method. */
  const nghttp3_nv connectprotoget_reqnv[] = {
      MAKE_NV(":scheme", "https"),       MAKE_NV(":path", "/"),
      MAKE_NV(":method", "GET"),         MAKE_NV(":authority", "localhost"),
      MAKE_NV(":protocol", "websocket"),
  };
  /* CONNECT method with :protocol requires :path. */
  const nghttp3_nv connectprotonopath_reqnv[] = {
      MAKE_NV(":scheme", "https"),
      MAKE_NV(":method", "CONNECT"),
      MAKE_NV(":authority", "localhost"),
      MAKE_NV(":protocol", "websocket"),
  };
  /* CONNECT method with :protocol requires :authority. */
  const nghttp3_nv connectprotonoauth_reqnv[] = {
      MAKE_NV(":scheme", "http"),        MAKE_NV(":path", "/"),
      MAKE_NV(":method", "CONNECT"),     MAKE_NV("host", "localhost"),
      MAKE_NV(":protocol", "websocket"),
  };
  /* regular CONNECT method should succeed with
     SETTINGS_CONNECT_PROTOCOL */
  const nghttp3_nv regularconnect_reqnv[] = {
      MAKE_NV(":method", "CONNECT"),
      MAKE_NV(":authority", "localhost"),
  };

  /* request header has no :path */
  check_http_req_header(nopath_reqnv, nghttp3_arraylen(nopath_reqnv),
                        NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
  check_http_req_header(earlyconnect_reqnv,
                        nghttp3_arraylen(earlyconnect_reqnv),
                        NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
  check_http_req_header(lateconnect_reqnv, nghttp3_arraylen(lateconnect_reqnv),
                        NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
  check_http_req_header(duppath_reqnv, nghttp3_arraylen(duppath_reqnv),
                        NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
  check_http_req_header(badcl_reqnv, nghttp3_arraylen(badcl_reqnv),
                        NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
  check_http_req_header(dupcl_reqnv, nghttp3_arraylen(dupcl_reqnv),
                        NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
  check_http_req_header(badhd_reqnv, nghttp3_arraylen(badhd_reqnv),
                        NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
  check_http_req_header(badauthority_reqnv,
                        nghttp3_arraylen(badauthority_reqnv),
                        NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
  check_http_req_header(badhdbtw_reqnv, nghttp3_arraylen(badhdbtw_reqnv),
                        NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
  check_http_req_header(asteriskget1_reqnv,
                        nghttp3_arraylen(asteriskget1_reqnv),
                        NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
  check_http_req_header(asteriskget2_reqnv,
                        nghttp3_arraylen(asteriskget2_reqnv),
                        NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
  check_http_req_header(asteriskoptions1_reqnv,
                        nghttp3_arraylen(asteriskoptions1_reqnv), 0);
  check_http_req_header(asteriskoptions2_reqnv,
                        nghttp3_arraylen(asteriskoptions2_reqnv), 0);
  check_http_req_header(invalidname_reqnv, nghttp3_arraylen(invalidname_reqnv),
                        NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
  check_http_req_header(invalidvalue_reqnv,
                        nghttp3_arraylen(invalidvalue_reqnv),
                        NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
  check_http_req_header(connectproto_reqnv,
                        nghttp3_arraylen(connectproto_reqnv),
                        NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
  check_http_req_header(connectprotoget_reqnv,
                        nghttp3_arraylen(connectprotoget_reqnv),
                        NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
  check_http_req_header(connectprotonopath_reqnv,
                        nghttp3_arraylen(connectprotonopath_reqnv),
                        NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
  check_http_req_header(connectprotonoauth_reqnv,
                        nghttp3_arraylen(connectprotonoauth_reqnv),
                        NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
  check_http_req_header(regularconnect_reqnv,
                        nghttp3_arraylen(regularconnect_reqnv), 0);
}

void test_nghttp3_conn_http_content_length(void) {
  uint8_t rawbuf[4096];
  nghttp3_buf buf;
  nghttp3_frame_headers fr;
  nghttp3_conn *conn;
  nghttp3_conn_callbacks callbacks;
  nghttp3_conn_settings settings;
  const nghttp3_mem *mem = nghttp3_mem_default();
  ssize_t sconsumed;
  nghttp3_qpack_encoder qenc;
  nghttp3_stream *stream;
  const nghttp3_nv reqnv[] = {
      MAKE_NV(":path", "/"),        MAKE_NV(":method", "PUT"),
      MAKE_NV(":scheme", "https"),  MAKE_NV("te", "trailers"),
      MAKE_NV("host", "localhost"), MAKE_NV("content-length", "9000000000"),
  };
  const nghttp3_nv resnv[] = {
      MAKE_NV(":status", "200"),
      MAKE_NV("te", "trailers"),
      MAKE_NV("content-length", "9000000000"),
  };

  memset(&callbacks, 0, sizeof(callbacks));
  nghttp3_conn_settings_default(&settings);

  /* client */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)resnv;
  fr.nvlen = nghttp3_arraylen(resnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  nghttp3_conn_client_new(&conn, &callbacks, &settings, mem, NULL);
  nghttp3_conn_create_stream(conn, &stream, 0);
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT((ssize_t)nghttp3_buf_len(&buf) == sconsumed);
  CU_ASSERT(9000000000LL == stream->rx.http.content_length);
  CU_ASSERT(200 == stream->rx.http.status_code);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* server */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)reqnv;
  fr.nvlen = nghttp3_arraylen(reqnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  nghttp3_conn_server_new(&conn, &callbacks, &settings, mem, NULL);
  nghttp3_conn_set_max_client_streams_bidi(conn, 1);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT((ssize_t)nghttp3_buf_len(&buf) == sconsumed);

  stream = nghttp3_conn_find_stream(conn, 0);

  CU_ASSERT(9000000000LL == stream->rx.http.content_length);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);
}

void test_nghttp3_conn_http_content_length_mismatch(void) {
  uint8_t rawbuf[4096];
  nghttp3_buf buf;
  nghttp3_frame_headers fr;
  nghttp3_conn *conn;
  nghttp3_conn_callbacks callbacks;
  nghttp3_conn_settings settings;
  const nghttp3_mem *mem = nghttp3_mem_default();
  ssize_t sconsumed;
  nghttp3_qpack_encoder qenc;
  const nghttp3_nv reqnv[] = {
      MAKE_NV(":path", "/"),
      MAKE_NV(":method", "PUT"),
      MAKE_NV(":authority", "localhost"),
      MAKE_NV(":scheme", "https"),
      MAKE_NV("content-length", "20"),
  };
  const nghttp3_nv resnv[] = {
      MAKE_NV(":status", "200"),
      MAKE_NV("content-length", "20"),
  };
  int rv;
  nghttp3_stream *stream;

  memset(&callbacks, 0, sizeof(callbacks));
  nghttp3_conn_settings_default(&settings);

  /* content-length is 20, but no DATA is present */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)reqnv;
  fr.nvlen = nghttp3_arraylen(reqnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  nghttp3_conn_server_new(&conn, &callbacks, &settings, mem, NULL);
  nghttp3_conn_set_max_client_streams_bidi(conn, 1);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT((ssize_t)nghttp3_buf_len(&buf) == sconsumed);

  rv = nghttp3_conn_close_stream(conn, 0, NGHTTP3_HTTP_NO_ERROR);

  CU_ASSERT(NGHTTP3_ERR_MALFORMED_HTTP_MESSAGING == rv);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* content-length is 20, but server receives 21 bytes DATA. */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)reqnv;
  fr.nvlen = nghttp3_arraylen(reqnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);
  nghttp3_write_frame_data(&buf, 21);

  nghttp3_conn_server_new(&conn, &callbacks, &settings, mem, NULL);
  nghttp3_conn_set_max_client_streams_bidi(conn, 1);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT(NGHTTP3_ERR_MALFORMED_HTTP_MESSAGING == sconsumed);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* Check client side as well */

  /* content-length is 20, but no DATA is present */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)resnv;
  fr.nvlen = nghttp3_arraylen(resnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  nghttp3_conn_client_new(&conn, &callbacks, &settings, mem, NULL);
  nghttp3_conn_create_stream(conn, &stream, 0);
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT((ssize_t)nghttp3_buf_len(&buf) == sconsumed);

  rv = nghttp3_conn_close_stream(conn, 0, NGHTTP3_HTTP_NO_ERROR);

  CU_ASSERT(NGHTTP3_ERR_MALFORMED_HTTP_MESSAGING == rv);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* content-length is 20, but server receives 21 bytes DATA. */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)resnv;
  fr.nvlen = nghttp3_arraylen(resnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);
  nghttp3_write_frame_data(&buf, 21);

  nghttp3_conn_client_new(&conn, &callbacks, &settings, mem, NULL);
  nghttp3_conn_create_stream(conn, &stream, 0);
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT(NGHTTP3_ERR_MALFORMED_HTTP_MESSAGING == sconsumed);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);
}

void test_nghttp3_conn_http_non_final_response(void) {
  uint8_t rawbuf[4096];
  nghttp3_buf buf;
  nghttp3_frame_headers fr;
  nghttp3_conn *conn;
  nghttp3_conn_callbacks callbacks;
  nghttp3_conn_settings settings;
  const nghttp3_mem *mem = nghttp3_mem_default();
  ssize_t sconsumed;
  nghttp3_qpack_encoder qenc;
  const nghttp3_nv infonv[] = {
      MAKE_NV(":status", "103"),
  };
  const nghttp3_nv resnv[] = {
      MAKE_NV(":status", "204"),
  };
  const nghttp3_nv trnv[] = {
      MAKE_NV("my-status", "ok"),
  };
  nghttp3_stream *stream;

  memset(&callbacks, 0, sizeof(callbacks));
  nghttp3_conn_settings_default(&settings);

  /* non-final followed by DATA is illegal.  */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)infonv;
  fr.nvlen = nghttp3_arraylen(infonv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);
  nghttp3_write_frame_data(&buf, 0);

  nghttp3_conn_client_new(&conn, &callbacks, &settings, mem, NULL);
  nghttp3_conn_create_stream(conn, &stream, 0);
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT(NGHTTP3_ERR_HTTP_FRAME_UNEXPECTED == sconsumed);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* 2 non-finals followed by final headers */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)infonv;
  fr.nvlen = nghttp3_arraylen(infonv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);
  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)resnv;
  fr.nvlen = nghttp3_arraylen(resnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  nghttp3_conn_client_new(&conn, &callbacks, &settings, mem, NULL);
  nghttp3_conn_create_stream(conn, &stream, 0);
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT((ssize_t)nghttp3_buf_len(&buf) == sconsumed);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* non-finals followed by trailers */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)infonv;
  fr.nvlen = nghttp3_arraylen(infonv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)trnv;
  fr.nvlen = nghttp3_arraylen(trnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  nghttp3_conn_client_new(&conn, &callbacks, &settings, mem, NULL);
  nghttp3_conn_create_stream(conn, &stream, 0);
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT(NGHTTP3_ERR_MALFORMED_HTTP_HEADER == sconsumed);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);
}

void test_nghttp3_conn_http_trailers(void) {
  uint8_t rawbuf[4096];
  nghttp3_buf buf;
  nghttp3_frame_headers fr;
  nghttp3_conn *conn;
  nghttp3_conn_callbacks callbacks;
  nghttp3_conn_settings settings;
  const nghttp3_mem *mem = nghttp3_mem_default();
  ssize_t sconsumed;
  nghttp3_qpack_encoder qenc;
  const nghttp3_nv reqnv[] = {
      MAKE_NV(":path", "/"),
      MAKE_NV(":method", "PUT"),
      MAKE_NV(":authority", "localhost"),
      MAKE_NV(":scheme", "https"),
  };
  const nghttp3_nv connect_reqnv[] = {
      MAKE_NV(":method", "CONNECT"),
      MAKE_NV(":authority", "localhost"),
  };
  const nghttp3_nv resnv[] = {
      MAKE_NV(":status", "200"),
  };
  const nghttp3_nv trnv[] = {
      MAKE_NV("foo", "bar"),
  };
  nghttp3_stream *stream;

  memset(&callbacks, 0, sizeof(callbacks));
  nghttp3_conn_settings_default(&settings);

  /* final response followed by trailers */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)resnv;
  fr.nvlen = nghttp3_arraylen(resnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)trnv;
  fr.nvlen = nghttp3_arraylen(trnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  nghttp3_conn_client_new(&conn, &callbacks, &settings, mem, NULL);
  nghttp3_conn_create_stream(conn, &stream, 0);
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT((ssize_t)nghttp3_buf_len(&buf) == sconsumed);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* trailers contain :status */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)resnv;
  fr.nvlen = nghttp3_arraylen(resnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)resnv;
  fr.nvlen = nghttp3_arraylen(resnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  nghttp3_conn_client_new(&conn, &callbacks, &settings, mem, NULL);
  nghttp3_conn_create_stream(conn, &stream, 0);
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT(NGHTTP3_ERR_MALFORMED_HTTP_HEADER == sconsumed);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* Receiving 2 trailers HEADERS is invalid*/
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)resnv;
  fr.nvlen = nghttp3_arraylen(resnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)trnv;
  fr.nvlen = nghttp3_arraylen(trnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);
  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  nghttp3_conn_client_new(&conn, &callbacks, &settings, mem, NULL);
  nghttp3_conn_create_stream(conn, &stream, 0);
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT(NGHTTP3_ERR_HTTP_GENERAL_PROTOCOL_ERROR == sconsumed);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* We don't expect response trailers after HEADERS with CONNECT
     request */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)resnv;
  fr.nvlen = nghttp3_arraylen(resnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)trnv;
  fr.nvlen = nghttp3_arraylen(trnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  nghttp3_conn_client_new(&conn, &callbacks, &settings, mem, NULL);
  nghttp3_conn_create_stream(conn, &stream, 0);
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;
  nghttp3_http_record_request_method(stream, connect_reqnv,
                                     nghttp3_arraylen(connect_reqnv));

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT(NGHTTP3_ERR_HTTP_FRAME_UNEXPECTED == sconsumed);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* We don't expect response trailers after DATA with CONNECT
     request */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)resnv;
  fr.nvlen = nghttp3_arraylen(resnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);
  nghttp3_write_frame_data(&buf, 99);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)trnv;
  fr.nvlen = nghttp3_arraylen(trnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  nghttp3_conn_client_new(&conn, &callbacks, &settings, mem, NULL);
  nghttp3_conn_create_stream(conn, &stream, 0);
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;
  nghttp3_http_record_request_method(stream, connect_reqnv,
                                     nghttp3_arraylen(connect_reqnv));

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT(NGHTTP3_ERR_HTTP_FRAME_UNEXPECTED == sconsumed);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* request followed by trailers */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)reqnv;
  fr.nvlen = nghttp3_arraylen(reqnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)trnv;
  fr.nvlen = nghttp3_arraylen(trnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  nghttp3_conn_server_new(&conn, &callbacks, &settings, mem, NULL);
  nghttp3_conn_set_max_client_streams_bidi(conn, 1);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT((ssize_t)nghttp3_buf_len(&buf) == sconsumed);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* request followed by trailers which contains pseudo headers */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)reqnv;
  fr.nvlen = nghttp3_arraylen(reqnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);
  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  nghttp3_conn_server_new(&conn, &callbacks, &settings, mem, NULL);
  nghttp3_conn_set_max_client_streams_bidi(conn, 1);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT(NGHTTP3_ERR_MALFORMED_HTTP_HEADER == sconsumed);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* request followed by 2 trailers */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)reqnv;
  fr.nvlen = nghttp3_arraylen(reqnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)trnv;
  fr.nvlen = nghttp3_arraylen(trnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);
  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  nghttp3_conn_server_new(&conn, &callbacks, &settings, mem, NULL);
  nghttp3_conn_set_max_client_streams_bidi(conn, 1);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT(NGHTTP3_ERR_HTTP_FRAME_UNEXPECTED == sconsumed);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* We don't expect trailers after HEADERS with CONNECT request */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)connect_reqnv;
  fr.nvlen = nghttp3_arraylen(connect_reqnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)trnv;
  fr.nvlen = nghttp3_arraylen(trnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  nghttp3_conn_server_new(&conn, &callbacks, &settings, mem, NULL);
  nghttp3_conn_set_max_client_streams_bidi(conn, 1);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT(NGHTTP3_ERR_HTTP_FRAME_UNEXPECTED == sconsumed);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* We don't expect trailers after DATA with CONNECT request */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)connect_reqnv;
  fr.nvlen = nghttp3_arraylen(connect_reqnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);
  nghttp3_write_frame_data(&buf, 11);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)trnv;
  fr.nvlen = nghttp3_arraylen(trnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  nghttp3_conn_server_new(&conn, &callbacks, &settings, mem, NULL);
  nghttp3_conn_set_max_client_streams_bidi(conn, 1);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT(NGHTTP3_ERR_HTTP_FRAME_UNEXPECTED == sconsumed);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);
}

void test_nghttp3_conn_http_ignore_content_length(void) {
  uint8_t rawbuf[4096];
  nghttp3_buf buf;
  nghttp3_frame_headers fr;
  nghttp3_conn *conn;
  nghttp3_conn_callbacks callbacks;
  nghttp3_conn_settings settings;
  const nghttp3_mem *mem = nghttp3_mem_default();
  ssize_t sconsumed;
  nghttp3_qpack_encoder qenc;
  const nghttp3_nv reqnv[] = {
      MAKE_NV(":authority", "localhost"),
      MAKE_NV(":method", "CONNECT"),
      MAKE_NV("content-length", "999999"),
  };
  const nghttp3_nv resnv[] = {
      MAKE_NV(":status", "304"),
      MAKE_NV("content-length", "20"),
  };
  const nghttp3_nv cl_resnv[] = {
      MAKE_NV(":status", "200"),
      MAKE_NV("content-length", "0"),
  };
  int rv;
  nghttp3_stream *stream;

  memset(&callbacks, 0, sizeof(callbacks));
  nghttp3_conn_settings_default(&settings);

  /* If status code is 304, content-length must be ignored. */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)resnv;
  fr.nvlen = nghttp3_arraylen(resnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  nghttp3_conn_client_new(&conn, &callbacks, &settings, mem, NULL);
  nghttp3_conn_create_stream(conn, &stream, 0);
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT((ssize_t)nghttp3_buf_len(&buf) == sconsumed);
  CU_ASSERT(0 == stream->rx.http.content_length);

  rv = nghttp3_conn_close_stream(conn, 0, NGHTTP3_HTTP_NO_ERROR);

  CU_ASSERT(0 == rv);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* If method is CONNECT, content-length must be ignored. */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)reqnv;
  fr.nvlen = nghttp3_arraylen(reqnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  nghttp3_conn_server_new(&conn, &callbacks, &settings, mem, NULL);
  nghttp3_conn_set_max_client_streams_bidi(conn, 1);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT((ssize_t)nghttp3_buf_len(&buf) == sconsumed);

  stream = nghttp3_conn_find_stream(conn, 0);

  CU_ASSERT(-1 == stream->rx.http.content_length);

  rv = nghttp3_conn_close_stream(conn, 0, NGHTTP3_HTTP_NO_ERROR);

  CU_ASSERT(0 == rv);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* Content-Length in 200 response to CONNECT is ignored */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)cl_resnv;
  fr.nvlen = nghttp3_arraylen(cl_resnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  nghttp3_conn_client_new(&conn, &callbacks, &settings, mem, NULL);
  nghttp3_conn_create_stream(conn, &stream, 0);
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;
  stream->rx.http.flags |= NGHTTP3_HTTP_FLAG_METH_CONNECT;

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT((ssize_t)nghttp3_buf_len(&buf) == sconsumed);
  CU_ASSERT(-1 == stream->rx.http.content_length);

  rv = nghttp3_conn_close_stream(conn, 0, NGHTTP3_HTTP_NO_ERROR);

  CU_ASSERT(0 == rv);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);
}

void test_nghttp3_conn_http_record_request_method(void) {
  uint8_t rawbuf[4096];
  nghttp3_buf buf;
  nghttp3_frame_headers fr;
  nghttp3_conn *conn;
  nghttp3_conn_callbacks callbacks;
  nghttp3_conn_settings settings;
  const nghttp3_mem *mem = nghttp3_mem_default();
  ssize_t sconsumed;
  nghttp3_qpack_encoder qenc;
  const nghttp3_nv connect_reqnv[] = {
      MAKE_NV(":authority", "localhost"),
      MAKE_NV(":method", "CONNECT"),
  };
  const nghttp3_nv head_reqnv[] = {
      MAKE_NV(":authority", "localhost"),
      MAKE_NV(":method", "HEAD"),
      MAKE_NV(":scheme", "https"),
      MAKE_NV(":path", "/"),
  };
  const nghttp3_nv resnv[] = {
      MAKE_NV(":status", "200"),
      MAKE_NV("content-length", "1000000007"),
  };
  nghttp3_stream *stream;

  memset(&callbacks, 0, sizeof(callbacks));
  nghttp3_conn_settings_default(&settings);

  /* content-length is not allowed with 200 status code in response to
     CONNECT request.  Just ignore it. */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)resnv;
  fr.nvlen = nghttp3_arraylen(resnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  nghttp3_conn_client_new(&conn, &callbacks, &settings, mem, NULL);
  nghttp3_conn_create_stream(conn, &stream, 0);
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;
  nghttp3_http_record_request_method(stream, connect_reqnv,
                                     nghttp3_arraylen(connect_reqnv));

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT((ssize_t)nghttp3_buf_len(&buf) == sconsumed);
  CU_ASSERT(-1 == stream->rx.http.content_length);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* The content-length in response to HEAD request must be
     ignored. */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)resnv;
  fr.nvlen = nghttp3_arraylen(resnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  nghttp3_conn_client_new(&conn, &callbacks, &settings, mem, NULL);
  nghttp3_conn_create_stream(conn, &stream, 0);
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;
  nghttp3_http_record_request_method(stream, head_reqnv,
                                     nghttp3_arraylen(head_reqnv));

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT((ssize_t)nghttp3_buf_len(&buf) == sconsumed);
  CU_ASSERT(0 == stream->rx.http.content_length);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);
}

void test_nghttp3_conn_qpack_blocked_stream(void) {
  const nghttp3_mem *mem = nghttp3_mem_default();
  nghttp3_conn *conn;
  nghttp3_conn_callbacks callbacks;
  nghttp3_conn_settings settings;
  nghttp3_qpack_encoder qenc;
  int rv;
  nghttp3_buf ebuf;
  uint8_t rawbuf[4096];
  nghttp3_buf buf;
  const nghttp3_nv reqnv[] = {
      MAKE_NV(":authority", "localhost"),
      MAKE_NV(":method", "GET"),
      MAKE_NV(":path", "/"),
      MAKE_NV(":scheme", "https"),
  };
  const nghttp3_nv resnv[] = {
      MAKE_NV(":status", "200"),
      MAKE_NV("server", "nghttp3"),
  };
  nghttp3_frame fr;
  ssize_t sconsumed;
  nghttp3_stream *stream;

  memset(&callbacks, 0, sizeof(callbacks));
  nghttp3_conn_settings_default(&settings);
  settings.qpack_max_table_capacity = 4096;
  settings.qpack_blocked_streams = 100;
  nghttp3_qpack_encoder_init(&qenc, settings.qpack_max_table_capacity,
                             settings.qpack_blocked_streams, mem);
  nghttp3_qpack_encoder_set_max_dtable_size(&qenc,
                                            settings.qpack_max_table_capacity);

  nghttp3_buf_init(&ebuf);
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));

  /* The deletion of QPACK blocked stream is deferred to the moment
     when it is unblocked */
  nghttp3_conn_client_new(&conn, &callbacks, &settings, mem, NULL);
  nghttp3_conn_bind_qpack_streams(conn, 2, 6);

  rv = nghttp3_conn_submit_request(conn, 0, reqnv, nghttp3_arraylen(reqnv),
                                   NULL, NULL);

  CU_ASSERT(0 == rv);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.headers.nva = (nghttp3_nv *)resnv;
  fr.headers.nvlen = nghttp3_arraylen(resnv);

  nghttp3_write_frame_qpack_dyn(&buf, &ebuf, &qenc, 0, &fr);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 1);

  CU_ASSERT(sconsumed > 0);
  CU_ASSERT(sconsumed != (ssize_t)nghttp3_buf_len(&buf));

  rv = nghttp3_conn_close_stream(conn, 0, NGHTTP3_HTTP_NO_ERROR);

  CU_ASSERT(0 == rv);

  stream = nghttp3_conn_find_stream(conn, 0);

  CU_ASSERT(stream->flags & NGHTTP3_STREAM_FLAG_CLOSED);

  nghttp3_buf_reset(&buf);
  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_QPACK_ENCODER);

  sconsumed = nghttp3_conn_read_stream(conn, 7, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT(sconsumed == (ssize_t)nghttp3_buf_len(&buf));
  CU_ASSERT(NULL != nghttp3_conn_find_stream(conn, 0));

  sconsumed = nghttp3_conn_read_stream(conn, 7, ebuf.pos,
                                       nghttp3_buf_len(&ebuf), /* fin = */ 0);

  CU_ASSERT(sconsumed == (ssize_t)nghttp3_buf_len(&ebuf));
  CU_ASSERT(NULL == nghttp3_conn_find_stream(conn, 0));

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);
  nghttp3_buf_free(&ebuf, mem);
}

void test_nghttp3_conn_recv_cancel_push(void) {
  const nghttp3_mem *mem = nghttp3_mem_default();
  nghttp3_conn *conn;
  nghttp3_conn_callbacks callbacks;
  nghttp3_conn_settings settings;
  nghttp3_stream *stream;
  nghttp3_frame fr;
  const nghttp3_nv reqnv[] = {
      MAKE_NV(":scheme", "https"),
      MAKE_NV(":authority", "localhost"),
      MAKE_NV(":path", "/"),
      MAKE_NV(":method", "GET"),
  };
  int rv;
  int64_t push_id;
  uint8_t rawbuf[4096], rawbuf2[4096];
  nghttp3_buf buf, buf2;
  ssize_t sconsumed;
  nghttp3_push_promise *pp;
  nghttp3_qpack_encoder qenc;
  userdata ud;

  memset(&ud, 0, sizeof(ud));
  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.cancel_push = cancel_push;
  nghttp3_conn_settings_default(&settings);

  /* Client cancels push before server binds stream ID to push ID */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_conn_server_new(&conn, &callbacks, &settings, mem, &ud);
  nghttp3_conn_bind_qpack_streams(conn, 7, 11);
  nghttp3_conn_set_max_client_streams_bidi(conn, 1);
  conn->local.uni.max_pushes = 1;
  nghttp3_conn_create_stream(conn, &stream, 0);

  rv = nghttp3_conn_submit_push_promise(conn, &push_id, 0, reqnv,
                                        nghttp3_arraylen(reqnv));

  CU_ASSERT(0 == rv);

  pp = nghttp3_conn_find_push_promise(conn, 0);

  CU_ASSERT(NULL != pp);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  fr.hd.type = NGHTTP3_FRAME_SETTINGS;
  fr.settings.niv = 0;

  nghttp3_write_frame(&buf, &fr);

  fr.hd.type = NGHTTP3_FRAME_CANCEL_PUSH;
  fr.cancel_push.push_id = push_id;

  nghttp3_write_frame(&buf, &fr);

  sconsumed = nghttp3_conn_read_stream(conn, 2, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT((ssize_t)nghttp3_buf_len(&buf) == sconsumed);

  pp = nghttp3_conn_find_push_promise(conn, 0);

  CU_ASSERT(NULL == pp);

  nghttp3_conn_del(conn);

  /* Client cancels push after server binds stream ID to push ID */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_conn_server_new(&conn, &callbacks, &settings, mem, &ud);
  nghttp3_conn_bind_qpack_streams(conn, 7, 11);
  nghttp3_conn_set_max_client_streams_bidi(conn, 1);
  conn->local.uni.max_pushes = 1;
  nghttp3_conn_create_stream(conn, &stream, 0);

  rv = nghttp3_conn_submit_push_promise(conn, &push_id, 0, reqnv,
                                        nghttp3_arraylen(reqnv));

  CU_ASSERT(0 == rv);

  rv = nghttp3_conn_bind_push_stream(conn, push_id, 15);

  CU_ASSERT(0 == rv);

  stream = nghttp3_conn_find_stream(conn, 15);

  CU_ASSERT(NULL != stream);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  fr.hd.type = NGHTTP3_FRAME_SETTINGS;
  fr.settings.niv = 0;

  nghttp3_write_frame(&buf, &fr);

  fr.hd.type = NGHTTP3_FRAME_CANCEL_PUSH;
  fr.cancel_push.push_id = push_id;

  nghttp3_write_frame(&buf, &fr);

  sconsumed = nghttp3_conn_read_stream(conn, 2, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT((ssize_t)nghttp3_buf_len(&buf) == sconsumed);

  pp = nghttp3_conn_find_push_promise(conn, 0);

  CU_ASSERT(NULL == pp);

  stream = nghttp3_conn_find_stream(conn, 15);

  CU_ASSERT(NULL == stream);

  nghttp3_conn_del(conn);

  /* Server cancels push after PUSH_PROMISE is completely received by
     client */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, 0, mem);
  nghttp3_conn_client_new(&conn, &callbacks, &settings, mem, &ud);
  conn->remote.uni.max_pushes = 1;
  nghttp3_conn_create_stream(conn, &stream, 0);
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;

  fr.hd.type = NGHTTP3_FRAME_PUSH_PROMISE;
  fr.push_promise.push_id = 0;
  fr.push_promise.nva = (nghttp3_nv *)reqnv;
  fr.push_promise.nvlen = nghttp3_arraylen(reqnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, &fr);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT((ssize_t)nghttp3_buf_len(&buf) == sconsumed);

  pp = nghttp3_conn_find_push_promise(conn, 0);

  CU_ASSERT(NULL != pp);

  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);
  fr.hd.type = NGHTTP3_FRAME_SETTINGS;
  fr.settings.niv = 0;

  nghttp3_write_frame(&buf, &fr);

  fr.hd.type = NGHTTP3_FRAME_CANCEL_PUSH;
  fr.cancel_push.push_id = 0;

  nghttp3_write_frame(&buf, &fr);

  ud.cancel_push_cb.ncalled = 0;
  ud.cancel_push_cb.push_id = -1;
  sconsumed = nghttp3_conn_read_stream(conn, 3, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT((ssize_t)nghttp3_buf_len(&buf) == sconsumed);
  CU_ASSERT(1 == ud.cancel_push_cb.ncalled);
  CU_ASSERT(0 == ud.cancel_push_cb.push_id);

  pp = nghttp3_conn_find_push_promise(conn, 0);

  CU_ASSERT(NULL == pp);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* Server cancels push before client starts receiving PUSH_PROMISE */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, 0, mem);
  nghttp3_conn_client_new(&conn, &callbacks, &settings, mem, &ud);
  conn->remote.uni.max_pushes = 1;
  nghttp3_conn_create_stream(conn, &stream, 0);
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);
  fr.hd.type = NGHTTP3_FRAME_SETTINGS;
  fr.settings.niv = 0;

  nghttp3_write_frame(&buf, &fr);

  fr.hd.type = NGHTTP3_FRAME_CANCEL_PUSH;
  fr.cancel_push.push_id = 0;

  nghttp3_write_frame(&buf, &fr);

  ud.cancel_push_cb.ncalled = 0;
  sconsumed = nghttp3_conn_read_stream(conn, 3, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT((ssize_t)nghttp3_buf_len(&buf) == sconsumed);
  CU_ASSERT(0 == ud.cancel_push_cb.ncalled);

  pp = nghttp3_conn_find_push_promise(conn, 0);

  CU_ASSERT(NULL != pp);
  CU_ASSERT(!pp->stream);

  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  fr.hd.type = NGHTTP3_FRAME_PUSH_PROMISE;
  fr.push_promise.push_id = 0;
  fr.push_promise.nva = (nghttp3_nv *)reqnv;
  fr.push_promise.nvlen = nghttp3_arraylen(reqnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, &fr);

  ud.cancel_push_cb.ncalled = 0;
  ud.cancel_push_cb.push_id = -1;
  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT((ssize_t)nghttp3_buf_len(&buf) == sconsumed);
  CU_ASSERT(1 == ud.cancel_push_cb.ncalled);
  CU_ASSERT(0 == ud.cancel_push_cb.push_id);

  pp = nghttp3_conn_find_push_promise(conn, 0);

  CU_ASSERT(NULL == pp);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* Server cancels push before client finishes reading PUSH_PROMISE */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, 0, mem);
  nghttp3_conn_client_new(&conn, &callbacks, &settings, mem, &ud);
  conn->remote.uni.max_pushes = 1;
  nghttp3_conn_create_stream(conn, &stream, 0);
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;

  fr.hd.type = NGHTTP3_FRAME_PUSH_PROMISE;
  fr.push_promise.push_id = 0;
  fr.push_promise.nva = (nghttp3_nv *)reqnv;
  fr.push_promise.nvlen = nghttp3_arraylen(reqnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, &fr);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, 3, /* fin = */ 0);

  CU_ASSERT(3 == sconsumed);

  pp = nghttp3_conn_find_push_promise(conn, 0);

  CU_ASSERT(NULL != pp);

  nghttp3_buf_wrap_init(&buf2, rawbuf2, sizeof(rawbuf2));
  buf2.last = nghttp3_put_varint(buf2.last, NGHTTP3_STREAM_TYPE_CONTROL);
  fr.hd.type = NGHTTP3_FRAME_SETTINGS;
  fr.settings.niv = 0;

  nghttp3_write_frame(&buf2, &fr);

  fr.hd.type = NGHTTP3_FRAME_CANCEL_PUSH;
  fr.cancel_push.push_id = 0;

  nghttp3_write_frame(&buf2, &fr);

  ud.cancel_push_cb.ncalled = 0;
  sconsumed =
      nghttp3_conn_read_stream(conn, 3, buf2.pos, nghttp3_buf_len(&buf2),
                               /* fin = */ 0);

  CU_ASSERT((ssize_t)nghttp3_buf_len(&buf2) == sconsumed);
  CU_ASSERT(0 == ud.cancel_push_cb.ncalled);

  pp = nghttp3_conn_find_push_promise(conn, 0);

  CU_ASSERT(NULL != pp);
  CU_ASSERT(pp->flags & NGHTTP3_PUSH_PROMISE_FLAG_RECV_CANCEL);
  CU_ASSERT(!(pp->flags & NGHTTP3_PUSH_PROMISE_FLAG_RECVED));

  ud.cancel_push_cb.ncalled = 0;
  ud.cancel_push_cb.push_id = -1;
  sconsumed = nghttp3_conn_read_stream(
      conn, 0, buf.pos + 3, nghttp3_buf_len(&buf) - 3, /* fin = */ 0);

  CU_ASSERT((ssize_t)nghttp3_buf_len(&buf) - 3 == sconsumed);
  CU_ASSERT(1 == ud.cancel_push_cb.ncalled);
  CU_ASSERT(0 == ud.cancel_push_cb.push_id);

  pp = nghttp3_conn_find_push_promise(conn, 0);

  CU_ASSERT(NULL == pp);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* Server sends push response and cancels push before client starts
     receiving PUSH_PROMISE.  Cancelling push after committing
     response has no effect. */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, 0, mem);
  nghttp3_conn_client_new(&conn, &callbacks, &settings, mem, &ud);
  conn->remote.uni.max_pushes = 1;
  nghttp3_conn_create_stream(conn, &stream, 0);
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;

  /* push stream */
  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_PUSH);
  buf.last = nghttp3_put_varint(buf.last, 0);

  sconsumed = nghttp3_conn_read_stream(conn, 7, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 1);

  CU_ASSERT((ssize_t)nghttp3_buf_len(&buf) == sconsumed);

  pp = nghttp3_conn_find_push_promise(conn, 0);

  CU_ASSERT(NULL != pp);

  /* control stream */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);
  fr.hd.type = NGHTTP3_FRAME_SETTINGS;
  fr.settings.niv = 0;

  nghttp3_write_frame(&buf, &fr);

  fr.hd.type = NGHTTP3_FRAME_CANCEL_PUSH;
  fr.cancel_push.push_id = 0;

  nghttp3_write_frame(&buf, &fr);

  ud.cancel_push_cb.ncalled = 0;
  sconsumed = nghttp3_conn_read_stream(conn, 3, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT((ssize_t)nghttp3_buf_len(&buf) == sconsumed);
  CU_ASSERT(0 == ud.cancel_push_cb.ncalled);

  pp = nghttp3_conn_find_push_promise(conn, 0);

  CU_ASSERT(NULL != pp);
  CU_ASSERT(7 == pp->stream->node.nid.id);

  /* request stream */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  fr.hd.type = NGHTTP3_FRAME_PUSH_PROMISE;
  fr.push_promise.push_id = 0;
  fr.push_promise.nva = (nghttp3_nv *)reqnv;
  fr.push_promise.nvlen = nghttp3_arraylen(reqnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, &fr);

  ud.cancel_push_cb.ncalled = 0;
  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT((ssize_t)nghttp3_buf_len(&buf) == sconsumed);
  CU_ASSERT(0 == ud.cancel_push_cb.ncalled);
  CU_ASSERT(0 == ud.cancel_push_cb.push_id);

  pp = nghttp3_conn_find_push_promise(conn, 0);

  CU_ASSERT(NULL != pp);
  CU_ASSERT(pp->stream->flags & NGHTTP3_STREAM_FLAG_READ_EOF);

  nghttp3_conn_close_stream(conn, 7, NGHTTP3_HTTP_NO_ERROR);

  pp = nghttp3_conn_find_push_promise(conn, 0);

  CU_ASSERT(NULL == pp);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);
}

void test_nghttp3_conn_cancel_push(void) {
  const nghttp3_mem *mem = nghttp3_mem_default();
  nghttp3_conn *conn;
  nghttp3_conn_callbacks callbacks;
  nghttp3_conn_settings settings;
  nghttp3_stream *stream;
  const nghttp3_nv reqnv[] = {
      MAKE_NV(":scheme", "https"),
      MAKE_NV(":authority", "localhost"),
      MAKE_NV(":path", "/"),
      MAKE_NV(":method", "GET"),
  };
  const nghttp3_nv resnv[] = {
      MAKE_NV(":status", "200"),
  };
  int rv;
  int64_t push_id;
  nghttp3_push_promise *pp;
  nghttp3_vec vec[16];
  ssize_t sveccnt;
  int64_t stream_id;
  int fin;
  nghttp3_qpack_encoder qenc;
  userdata ud;
  uint8_t rawbuf[4096];
  nghttp3_buf buf;
  nghttp3_frame fr;
  ssize_t sconsumed;

  memset(&ud, 0, sizeof(ud));
  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.recv_data = recv_data;
  callbacks.push_stream = push_stream;
  nghttp3_conn_settings_default(&settings);

  /* Cancel push before binding stream ID to push ID */
  nghttp3_conn_server_new(&conn, &callbacks, &settings, mem, NULL);
  nghttp3_conn_bind_control_stream(conn, 3);
  nghttp3_conn_bind_qpack_streams(conn, 7, 11);
  nghttp3_conn_set_max_client_streams_bidi(conn, 1);
  conn->local.uni.max_pushes = 1;
  nghttp3_conn_create_stream(conn, &stream, 0);

  rv = nghttp3_conn_submit_push_promise(conn, &push_id, 0, reqnv,
                                        nghttp3_arraylen(reqnv));

  CU_ASSERT(0 == rv);

  pp = nghttp3_conn_find_push_promise(conn, 0);

  CU_ASSERT(NULL != pp);

  rv = nghttp3_conn_cancel_push(conn, push_id);

  CU_ASSERT(0 == rv);

  pp = nghttp3_conn_find_push_promise(conn, 0);

  CU_ASSERT(NULL == pp);

  nghttp3_conn_del(conn);

  /* Cancel push after binding stream ID to push ID */
  nghttp3_conn_server_new(&conn, &callbacks, &settings, mem, NULL);
  nghttp3_conn_bind_control_stream(conn, 3);
  nghttp3_conn_bind_qpack_streams(conn, 7, 11);
  nghttp3_conn_set_max_client_streams_bidi(conn, 1);
  conn->local.uni.max_pushes = 1;
  nghttp3_conn_create_stream(conn, &stream, 0);

  rv = nghttp3_conn_submit_push_promise(conn, &push_id, 0, reqnv,
                                        nghttp3_arraylen(reqnv));

  CU_ASSERT(0 == rv);

  rv = nghttp3_conn_bind_push_stream(conn, push_id, 15);

  CU_ASSERT(0 == rv);

  rv = nghttp3_conn_cancel_push(conn, push_id);

  CU_ASSERT(NGHTTP3_ERR_TOO_LATE == rv);

  pp = nghttp3_conn_find_push_promise(conn, 0);

  CU_ASSERT(NULL != pp);

  stream = nghttp3_conn_find_stream(conn, 15);

  CU_ASSERT(NULL != stream);

  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  CU_ASSERT(sveccnt > 0);
  CU_ASSERT(0 == nghttp3_ringbuf_len(&conn->tx.ctrl->frq));

  nghttp3_conn_del(conn);

  /* Client cancels push after receiving PUSH_PROMISE */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, 0, mem);
  nghttp3_conn_client_new(&conn, &callbacks, &settings, mem, &ud);
  conn->remote.uni.max_pushes = 1;
  nghttp3_conn_create_stream(conn, &stream, 0);
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;
  nghttp3_conn_bind_control_stream(conn, 2);

  fr.hd.type = NGHTTP3_FRAME_PUSH_PROMISE;
  fr.push_promise.push_id = 0;
  fr.push_promise.nva = (nghttp3_nv *)reqnv;
  fr.push_promise.nvlen = nghttp3_arraylen(reqnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, &fr);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT((ssize_t)nghttp3_buf_len(&buf) == sconsumed);

  pp = nghttp3_conn_find_push_promise(conn, 0);

  CU_ASSERT(NULL != pp);

  rv = nghttp3_conn_cancel_push(conn, 0);

  CU_ASSERT(0 == rv);

  pp = nghttp3_conn_find_push_promise(conn, 0);

  CU_ASSERT(NULL == pp);

  /* push stream must be ignored */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_PUSH);
  buf.last = nghttp3_put_varint(buf.last, 0);
  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.headers.nva = (nghttp3_nv *)resnv;
  fr.headers.nvlen = nghttp3_arraylen(resnv);
  nghttp3_write_frame_qpack(&buf, &qenc, 0, &fr);

  sconsumed = nghttp3_conn_read_stream(conn, 7, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT((ssize_t)nghttp3_buf_len(&buf) == sconsumed);

  stream = nghttp3_conn_find_stream(conn, 7);

  CU_ASSERT(NULL == stream->pp);
  CU_ASSERT(NGHTTP3_PUSH_STREAM_STATE_IGN_REST == stream->rstate.state);

  rv = nghttp3_conn_close_stream(conn, 7, NGHTTP3_HTTP_NO_ERROR);

  CU_ASSERT(0 == rv);
  CU_ASSERT(NULL == nghttp3_conn_find_stream(conn, 7));

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* Client cancels push before receiving PUSH_PROMISE completely */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, 0, mem);
  nghttp3_conn_client_new(&conn, &callbacks, &settings, mem, &ud);
  conn->remote.uni.max_pushes = 1;
  nghttp3_conn_create_stream(conn, &stream, 0);
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;
  nghttp3_conn_bind_control_stream(conn, 2);

  fr.hd.type = NGHTTP3_FRAME_PUSH_PROMISE;
  fr.push_promise.push_id = 0;
  fr.push_promise.nva = (nghttp3_nv *)reqnv;
  fr.push_promise.nvlen = nghttp3_arraylen(reqnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, &fr);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, 3,
                                       /* fin = */ 0);

  CU_ASSERT(3 == sconsumed);

  pp = nghttp3_conn_find_push_promise(conn, 0);

  CU_ASSERT(NULL != pp);

  rv = nghttp3_conn_cancel_push(conn, 0);

  CU_ASSERT(0 == rv);

  pp = nghttp3_conn_find_push_promise(conn, 0);

  CU_ASSERT(NULL != pp);

  sconsumed = nghttp3_conn_read_stream(
      conn, 0, buf.pos + 3, nghttp3_buf_len(&buf) - 3, /* fin = */ 0);

  CU_ASSERT((ssize_t)nghttp3_buf_len(&buf) - 3 == sconsumed);

  pp = nghttp3_conn_find_push_promise(conn, 0);

  CU_ASSERT(NULL == pp);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* Client cancels push after receiving PUSH_PROMISE and push stream */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, 0, mem);
  nghttp3_conn_client_new(&conn, &callbacks, &settings, mem, &ud);
  conn->remote.uni.max_pushes = 1;
  nghttp3_conn_create_stream(conn, &stream, 0);
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;
  nghttp3_conn_bind_control_stream(conn, 2);

  fr.hd.type = NGHTTP3_FRAME_PUSH_PROMISE;
  fr.push_promise.push_id = 0;
  fr.push_promise.nva = (nghttp3_nv *)reqnv;
  fr.push_promise.nvlen = nghttp3_arraylen(reqnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, &fr);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT((ssize_t)nghttp3_buf_len(&buf) == sconsumed);

  pp = nghttp3_conn_find_push_promise(conn, 0);

  CU_ASSERT(NULL != pp);

  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_PUSH);
  buf.last = nghttp3_put_varint(buf.last, 0);
  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.headers.nva = (nghttp3_nv *)resnv;
  fr.headers.nvlen = nghttp3_arraylen(resnv);
  nghttp3_write_frame_qpack(&buf, &qenc, 0, &fr);

  sconsumed = nghttp3_conn_read_stream(conn, 7, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT((ssize_t)nghttp3_buf_len(&buf) == sconsumed);

  rv = nghttp3_conn_cancel_push(conn, 0);

  CU_ASSERT(0 == rv);

  pp = nghttp3_conn_find_push_promise(conn, 0);

  CU_ASSERT(NULL != pp);

  stream = nghttp3_conn_find_stream(conn, 7);

  CU_ASSERT(NULL != stream);

  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_write_frame_data(&buf, 1024);

  ud.recv_data_cb.ncalled = 0;
  sconsumed = nghttp3_conn_read_stream(conn, 7, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT((ssize_t)nghttp3_buf_len(&buf) == sconsumed);
  CU_ASSERT(0 == ud.recv_data_cb.ncalled);

  rv = nghttp3_conn_close_stream(conn, 7, NGHTTP3_HTTP_NO_ERROR);

  CU_ASSERT(0 == rv);
  CU_ASSERT(NULL == nghttp3_conn_find_stream(conn, 7));

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* Client cancels push after receiving push stream but without
     receiving PUSH_PROMISE */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, 0, mem);
  nghttp3_conn_client_new(&conn, &callbacks, &settings, mem, &ud);
  conn->remote.uni.max_pushes = 1;
  nghttp3_conn_create_stream(conn, &stream, 0);
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;
  nghttp3_conn_bind_control_stream(conn, 2);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_PUSH);
  buf.last = nghttp3_put_varint(buf.last, 0);
  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.headers.nva = (nghttp3_nv *)resnv;
  fr.headers.nvlen = nghttp3_arraylen(resnv);
  nghttp3_write_frame_qpack(&buf, &qenc, 0, &fr);

  sconsumed = nghttp3_conn_read_stream(conn, 7, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT(2 == sconsumed);

  stream = nghttp3_conn_find_stream(conn, 7);

  CU_ASSERT(nghttp3_stream_get_buffered_datalen(stream) > 0);

  rv = nghttp3_conn_cancel_push(conn, 0);

  CU_ASSERT(0 == rv);
  CU_ASSERT(0 == nghttp3_stream_get_buffered_datalen(stream));

  pp = nghttp3_conn_find_push_promise(conn, 0);

  CU_ASSERT(NULL != pp);

  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_PUSH);
  buf.last = nghttp3_put_varint(buf.last, 0);
  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.headers.nva = (nghttp3_nv *)resnv;
  fr.headers.nvlen = nghttp3_arraylen(resnv);
  nghttp3_write_frame_qpack(&buf, &qenc, 0, &fr);

  ud.push_stream_cb.ncalled = 0;
  sconsumed = nghttp3_conn_read_stream(conn, 7, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT((ssize_t)nghttp3_buf_len(&buf) == sconsumed);
  CU_ASSERT(0 == ud.push_stream_cb.ncalled);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);
}

void test_nghttp3_conn_recv_push_promise(void) {
  const nghttp3_mem *mem = nghttp3_mem_default();
  nghttp3_conn *conn;
  nghttp3_conn_callbacks callbacks;
  nghttp3_conn_settings settings;
  uint8_t rawbuf[4096];
  nghttp3_buf buf;
  nghttp3_frame fr;
  const nghttp3_nv reqnv[] = {
      MAKE_NV(":path", "/"),
      MAKE_NV(":method", "GET"),
      MAKE_NV(":scheme", "https"),
      MAKE_NV(":authority", "example.com"),
  };
  nghttp3_qpack_encoder qenc;
  nghttp3_stream *stream;
  ssize_t sconsumed;
  nghttp3_push_promise *pp;

  memset(&callbacks, 0, sizeof(callbacks));
  nghttp3_conn_settings_default(&settings);

  /* Receive PUSH_PROMISE */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, 0, mem);
  nghttp3_conn_client_new(&conn, &callbacks, &settings, mem, NULL);
  conn->remote.uni.max_pushes = 1;
  nghttp3_conn_create_stream(conn, &stream, 0);
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;

  fr.hd.type = NGHTTP3_FRAME_PUSH_PROMISE;
  fr.push_promise.push_id = 0;
  fr.push_promise.nva = (nghttp3_nv *)reqnv;
  fr.push_promise.nvlen = nghttp3_arraylen(reqnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, &fr);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT((ssize_t)nghttp3_buf_len(&buf) == sconsumed);

  pp = nghttp3_conn_find_push_promise(conn, 0);

  CU_ASSERT((NGHTTP3_HTTP_FLAG_REQ_HEADERS | NGHTTP3_HTTP_FLAG__AUTHORITY) ==
            (pp->http.flags &
             (NGHTTP3_HTTP_FLAG_REQ_HEADERS | NGHTTP3_HTTP_FLAG__AUTHORITY)));

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* Receiving same push ID twice is illegal */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, 0, mem);
  nghttp3_conn_client_new(&conn, &callbacks, &settings, mem, NULL);
  conn->remote.uni.max_pushes = 1;
  nghttp3_conn_create_stream(conn, &stream, 0);
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;

  fr.hd.type = NGHTTP3_FRAME_PUSH_PROMISE;
  fr.push_promise.push_id = 0;
  fr.push_promise.nva = (nghttp3_nv *)reqnv;
  fr.push_promise.nvlen = nghttp3_arraylen(reqnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, &fr);
  nghttp3_write_frame_qpack(&buf, &qenc, 0, &fr);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT(NGHTTP3_ERR_HTTP_FRAME_ERROR == sconsumed);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* Receiving push ID which exceeds the limit is illegal */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, 0, mem);
  nghttp3_conn_client_new(&conn, &callbacks, &settings, mem, NULL);
  conn->remote.uni.max_pushes = 1;
  nghttp3_conn_create_stream(conn, &stream, 0);
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;

  fr.hd.type = NGHTTP3_FRAME_PUSH_PROMISE;
  fr.push_promise.push_id = 1;
  fr.push_promise.nva = (nghttp3_nv *)reqnv;
  fr.push_promise.nvlen = nghttp3_arraylen(reqnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, &fr);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT(NGHTTP3_ERR_HTTP_FRAME_ERROR == sconsumed);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);
}

void test_nghttp3_conn_recv_push_stream(void) {
  const nghttp3_mem *mem = nghttp3_mem_default();
  nghttp3_conn *conn;
  nghttp3_conn_callbacks callbacks;
  nghttp3_conn_settings settings;
  uint8_t rawbuf[4096];
  nghttp3_buf buf;
  nghttp3_frame fr;
  const nghttp3_nv reqnv[] = {
      MAKE_NV(":path", "/"),
      MAKE_NV(":method", "GET"),
      MAKE_NV(":scheme", "https"),
      MAKE_NV(":authority", "example.com"),
  };
  const nghttp3_nv resnv[] = {
      MAKE_NV(":status", "200"),
  };
  nghttp3_qpack_encoder qenc;
  nghttp3_stream *stream, *bidi_stream;
  ssize_t sconsumed;
  nghttp3_push_promise *pp;

  memset(&callbacks, 0, sizeof(callbacks));
  nghttp3_conn_settings_default(&settings);

  /* Receive PUSH_PROMISE and then push stream */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, 0, mem);
  nghttp3_conn_client_new(&conn, &callbacks, &settings, mem, NULL);
  conn->remote.uni.max_pushes = 1;
  nghttp3_conn_create_stream(conn, &stream, 0);
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;

  fr.hd.type = NGHTTP3_FRAME_PUSH_PROMISE;
  fr.push_promise.push_id = 0;
  fr.push_promise.nva = (nghttp3_nv *)reqnv;
  fr.push_promise.nvlen = nghttp3_arraylen(reqnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, &fr);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT((ssize_t)nghttp3_buf_len(&buf) == sconsumed);

  pp = nghttp3_conn_find_push_promise(conn, 0);

  CU_ASSERT((NGHTTP3_HTTP_FLAG_REQ_HEADERS | NGHTTP3_HTTP_FLAG__AUTHORITY) ==
            (pp->http.flags &
             (NGHTTP3_HTTP_FLAG_REQ_HEADERS | NGHTTP3_HTTP_FLAG__AUTHORITY)));

  /* push promise is fulfilled with stream 3 */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_PUSH);
  buf.last = nghttp3_put_varint(buf.last, 0);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.headers.nva = (nghttp3_nv *)resnv;
  fr.headers.nvlen = nghttp3_arraylen(resnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 3, &fr);

  sconsumed = nghttp3_conn_read_stream(conn, 3, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 1);

  CU_ASSERT((ssize_t)nghttp3_buf_len(&buf) == sconsumed);

  stream = nghttp3_conn_find_stream(conn, 3);

  CU_ASSERT(pp == stream->pp);
  CU_ASSERT(stream == pp->stream);
  CU_ASSERT(NGHTTP3_HTTP_FLAG__STATUS & stream->rx.http.flags);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* Receiving push stream prior to PUSH_PROMISE */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, 0, mem);
  nghttp3_conn_client_new(&conn, &callbacks, &settings, mem, NULL);
  conn->remote.uni.max_pushes = 1;
  nghttp3_conn_create_stream(conn, &stream, 0);
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_PUSH);
  buf.last = nghttp3_put_varint(buf.last, 0);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.headers.nva = (nghttp3_nv *)resnv;
  fr.headers.nvlen = nghttp3_arraylen(resnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 3, &fr);

  sconsumed = nghttp3_conn_read_stream(conn, 3, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 1);

  CU_ASSERT(/* stream type + push ID */ 2 == sconsumed);

  pp = nghttp3_conn_find_push_promise(conn, 0);

  CU_ASSERT(NULL != pp);
  CU_ASSERT(&conn->orphan_root == pp->node.parent);

  stream = nghttp3_conn_find_stream(conn, 3);

  CU_ASSERT(stream == pp->stream);
  CU_ASSERT(pp == stream->pp);
  CU_ASSERT(NGHTTP3_STREAM_FLAG_PUSH_PROMISE_BLOCKED & stream->flags);

  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));

  fr.hd.type = NGHTTP3_FRAME_PUSH_PROMISE;
  fr.push_promise.push_id = 0;
  fr.push_promise.nva = (nghttp3_nv *)reqnv;
  fr.push_promise.nvlen = nghttp3_arraylen(reqnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, &fr);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  CU_ASSERT((ssize_t)nghttp3_buf_len(&buf) == sconsumed);
  CU_ASSERT(stream == pp->stream);
  CU_ASSERT(pp == stream->pp);
  CU_ASSERT(!(stream->flags & NGHTTP3_STREAM_FLAG_PUSH_PROMISE_BLOCKED));
  CU_ASSERT(0 == nghttp3_ringbuf_len(&stream->inq));

  bidi_stream = nghttp3_conn_find_stream(conn, 0);

  CU_ASSERT(&bidi_stream->node == pp->node.parent);
  CU_ASSERT(NGHTTP3_DEFAULT_WEIGHT == pp->node.weight);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);
}

void test_nghttp3_conn_just_fin(void) {
  const nghttp3_mem *mem = nghttp3_mem_default();
  nghttp3_conn *conn;
  nghttp3_conn_callbacks callbacks;
  nghttp3_conn_settings settings;
  nghttp3_vec vec[256];
  ssize_t sveccnt;
  int rv;
  int64_t stream_id;
  const nghttp3_nv nva[] = {
      MAKE_NV(":path", "/"),
      MAKE_NV(":authority", "example.com"),
      MAKE_NV(":scheme", "https"),
      MAKE_NV(":method", "GET"),
  };
  nghttp3_data_reader dr;
  int fin;
  userdata ud;

  memset(&callbacks, 0, sizeof(callbacks));
  nghttp3_conn_settings_default(&settings);
  memset(&ud, 0, sizeof(ud));

  nghttp3_conn_client_new(&conn, &callbacks, &settings, mem, &ud);

  nghttp3_conn_bind_control_stream(conn, 2);
  nghttp3_conn_bind_qpack_streams(conn, 6, 10);

  /* Write control streams */
  for (;;) {
    sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                         nghttp3_arraylen(vec));

    CU_ASSERT(sveccnt >= 0);

    if (sveccnt == 0) {
      break;
    }

    rv = nghttp3_conn_add_write_offset(conn, stream_id,
                                       nghttp3_vec_len(vec, (size_t)sveccnt));

    CU_ASSERT(0 == rv);
  }

  /* No DATA frame header */
  dr.read_data = step_read_data;
  rv = nghttp3_conn_submit_request(conn, 0, nva, nghttp3_arraylen(nva), &dr,
                                   NULL);

  CU_ASSERT(0 == rv);

  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  CU_ASSERT(1 == sveccnt);
  CU_ASSERT(0 == stream_id);
  CU_ASSERT(1 == fin);

  rv = nghttp3_conn_add_write_offset(conn, stream_id,
                                     nghttp3_vec_len(vec, (size_t)sveccnt));

  CU_ASSERT(0 == rv);

  /* Just fin */
  ud.data.nblock = 1;
  dr.read_data = block_then_step_read_data;

  rv = nghttp3_conn_submit_request(conn, 4, nva, nghttp3_arraylen(nva), &dr,
                                   NULL);

  CU_ASSERT(0 == rv);

  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  CU_ASSERT(1 == sveccnt);
  CU_ASSERT(4 == stream_id);
  CU_ASSERT(0 == fin);

  rv = nghttp3_conn_add_write_offset(conn, stream_id,
                                     nghttp3_vec_len(vec, (size_t)sveccnt));

  CU_ASSERT(0 == rv);

  /* Resume stream 4 because it was blocked */
  nghttp3_conn_resume_stream(conn, 4);

  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  CU_ASSERT(0 == sveccnt);
  CU_ASSERT(4 == stream_id);
  CU_ASSERT(1 == fin);

  rv = nghttp3_conn_add_write_offset(conn, stream_id,
                                     nghttp3_vec_len(vec, (size_t)sveccnt));

  CU_ASSERT(0 == rv);

  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  CU_ASSERT(0 == sveccnt);
  CU_ASSERT(-1 == stream_id);
  CU_ASSERT(0 == fin);

  nghttp3_conn_del(conn);
}

void test_nghttp3_conn_submit_response_read_blocked(void) {
  const nghttp3_mem *mem = nghttp3_mem_default();
  nghttp3_conn *conn;
  nghttp3_conn_callbacks callbacks;
  nghttp3_conn_settings settings;
  const nghttp3_nv nva[] = {
      MAKE_NV(":status", "200"),
  };
  nghttp3_stream *stream;
  int rv;
  nghttp3_vec vec[256];
  int fin;
  int64_t stream_id;
  ssize_t sveccnt;
  nghttp3_data_reader dr = {step_then_block_read_data};
  userdata ud;

  memset(&callbacks, 0, sizeof(callbacks));
  nghttp3_conn_settings_default(&settings);
  memset(&ud, 0, sizeof(ud));

  /* Make sure that flushing serialized data while
     NGHTTP3_STREAM_FLAG_READ_DATA_BLOCKED is set does not cause any
     error */
  nghttp3_conn_server_new(&conn, &callbacks, &settings, mem, &ud);
  conn->remote.bidi.max_client_streams = 1;
  nghttp3_conn_bind_qpack_streams(conn, 7, 11);

  nghttp3_conn_create_stream(conn, &stream, 0);

  ud.data.left = 1000;
  ud.data.step = 1000;
  rv = nghttp3_conn_submit_response(conn, 0, nva, nghttp3_arraylen(nva), &dr);

  CU_ASSERT(0 == rv);

  for (;;) {
    sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                         nghttp3_arraylen(vec));

    CU_ASSERT(sveccnt >= 0);

    if (sveccnt <= 0) {
      break;
    }

    rv = nghttp3_conn_add_write_offset(conn, stream_id, 1);

    CU_ASSERT(0 == rv);
  }

  nghttp3_conn_del(conn);
}
