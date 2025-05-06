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

#include <stdio.h>
#include <assert.h>

#include "nghttp3_conn.h"
#include "nghttp3_macro.h"
#include "nghttp3_conv.h"
#include "nghttp3_frame.h"
#include "nghttp3_vec.h"
#include "nghttp3_test_helper.h"
#include "nghttp3_http.h"
#include "nghttp3_str.h"

static const MunitTest tests[] = {
  munit_void_test(test_nghttp3_conn_read_control),
  munit_void_test(test_nghttp3_conn_write_control),
  munit_void_test(test_nghttp3_conn_submit_request),
  munit_void_test(test_nghttp3_conn_http_request),
  munit_void_test(test_nghttp3_conn_http_resp_header),
  munit_void_test(test_nghttp3_conn_http_req_header),
  munit_void_test(test_nghttp3_conn_http_content_length),
  munit_void_test(test_nghttp3_conn_http_content_length_mismatch),
  munit_void_test(test_nghttp3_conn_http_non_final_response),
  munit_void_test(test_nghttp3_conn_http_trailers),
  munit_void_test(test_nghttp3_conn_http_ignore_content_length),
  munit_void_test(test_nghttp3_conn_http_record_request_method),
  munit_void_test(test_nghttp3_conn_http_error),
  munit_void_test(test_nghttp3_conn_qpack_blocked_stream),
  munit_void_test(test_nghttp3_conn_qpack_decoder_cancel_stream),
  munit_void_test(test_nghttp3_conn_just_fin),
  munit_void_test(test_nghttp3_conn_submit_response_read_blocked),
  munit_void_test(test_nghttp3_conn_submit_info),
  munit_void_test(test_nghttp3_conn_recv_uni),
  munit_void_test(test_nghttp3_conn_recv_goaway),
  munit_void_test(test_nghttp3_conn_shutdown_server),
  munit_void_test(test_nghttp3_conn_shutdown_client),
  munit_void_test(test_nghttp3_conn_priority_update),
  munit_void_test(test_nghttp3_conn_request_priority),
  munit_void_test(test_nghttp3_conn_set_stream_priority),
  munit_void_test(test_nghttp3_conn_shutdown_stream_read),
  munit_void_test(test_nghttp3_conn_stream_data_overflow),
  munit_void_test(test_nghttp3_conn_get_frame_payload_left),
  munit_void_test(test_nghttp3_conn_update_ack_offset),
  munit_void_test(test_nghttp3_conn_set_client_stream_priority),
  munit_void_test(test_nghttp3_conn_rx_http_state),
  munit_void_test(test_nghttp3_conn_push),
  munit_test_end(),
};

const MunitSuite conn_suite = {
  .prefix = "/conn",
  .tests = tests,
};

static uint8_t nulldata[4096];

static const nghttp3_nv req_nva[] = {
  MAKE_NV(":scheme", "https"),
  MAKE_NV(":method", "GET"),
  MAKE_NV(":authority", "example.com"),
  MAKE_NV(":path", "/"),
};

static const nghttp3_nv resp_nva[] = {
  MAKE_NV(":status", "200"),
  MAKE_NV("server", "nghttp3"),
};

typedef struct {
  struct {
    size_t nblock;
    size_t left;
    size_t step;
  } data;
  struct {
    size_t ncalled;
    uint64_t acc;
  } ack;
  struct {
    size_t ncalled;
    int64_t stream_id;
    uint64_t app_error_code;
  } stop_sending_cb;
  struct {
    size_t ncalled;
    int64_t stream_id;
    uint64_t app_error_code;
  } reset_stream_cb;
  struct {
    size_t ncalled;
    int64_t id;
  } shutdown_cb;
  struct {
    size_t consumed_total;
  } deferred_consume_cb;
  struct {
    size_t ncalled;
    nghttp3_settings settings;
  } recv_settings_cb;
  struct {
    size_t ncalled;
  } recv_trailer_cb;
} userdata;

static int acked_stream_data(nghttp3_conn *conn, int64_t stream_id,
                             uint64_t datalen, void *user_data,
                             void *stream_user_data) {
  userdata *ud = user_data;

  (void)conn;
  (void)stream_id;
  (void)stream_user_data;

  ++ud->ack.ncalled;
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

static int end_headers(nghttp3_conn *conn, int64_t stream_id, int fin,
                       void *user_data, void *stream_user_data) {
  (void)conn;
  (void)stream_id;
  (void)fin;
  (void)stream_user_data;
  (void)user_data;
  return 0;
}

static int recv_trailer(nghttp3_conn *conn, int64_t stream_id, int32_t token,
                        nghttp3_rcbuf *name, nghttp3_rcbuf *value,
                        uint8_t flags, void *user_data,
                        void *stream_user_data) {
  userdata *ud = user_data;
  (void)conn;
  (void)stream_id;
  (void)token;
  (void)name;
  (void)value;
  (void)flags;
  (void)stream_user_data;

  ++ud->recv_trailer_cb.ncalled;

  return 0;
}

static nghttp3_ssize empty_read_data(nghttp3_conn *conn, int64_t stream_id,
                                     nghttp3_vec *vec, size_t veccnt,
                                     uint32_t *pflags, void *user_data,
                                     void *stream_user_data) {
  (void)conn;
  (void)stream_id;
  (void)vec;
  (void)veccnt;
  (void)user_data;
  (void)stream_user_data;

  *pflags = NGHTTP3_DATA_FLAG_EOF | NGHTTP3_DATA_FLAG_NO_END_STREAM;

  return 0;
}

static nghttp3_ssize step_read_data(nghttp3_conn *conn, int64_t stream_id,
                                    nghttp3_vec *vec, size_t veccnt,
                                    uint32_t *pflags, void *user_data,
                                    void *stream_user_data) {
  userdata *ud = user_data;
  size_t n = nghttp3_min_size(ud->data.left, ud->data.step);

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

static nghttp3_ssize
block_then_step_read_data(nghttp3_conn *conn, int64_t stream_id,
                          nghttp3_vec *vec, size_t veccnt, uint32_t *pflags,
                          void *user_data, void *stream_user_data) {
  userdata *ud = user_data;

  if (ud->data.nblock == 0) {
    return step_read_data(conn, stream_id, vec, veccnt, pflags, user_data,
                          stream_user_data);
  }

  --ud->data.nblock;

  return NGHTTP3_ERR_WOULDBLOCK;
}

static nghttp3_ssize
step_then_block_read_data(nghttp3_conn *conn, int64_t stream_id,
                          nghttp3_vec *vec, size_t veccnt, uint32_t *pflags,
                          void *user_data, void *stream_user_data) {
  nghttp3_ssize rv;

  rv = step_read_data(conn, stream_id, vec, veccnt, pflags, user_data,
                      stream_user_data);

  assert(rv >= 0);

  if (*pflags & NGHTTP3_DATA_FLAG_EOF) {
    *pflags &= (uint32_t)~NGHTTP3_DATA_FLAG_EOF;

    if (nghttp3_vec_len(vec, (size_t)rv) == 0) {
      return NGHTTP3_ERR_WOULDBLOCK;
    }
  }

  return rv;
}

#if SIZE_MAX > UINT32_MAX
static nghttp3_ssize stream_data_overflow_read_data(
  nghttp3_conn *conn, int64_t stream_id, nghttp3_vec *vec, size_t veccnt,
  uint32_t *pflags, void *user_data, void *stream_user_data) {
  (void)conn;
  (void)stream_id;
  (void)veccnt;
  (void)pflags;
  (void)user_data;
  (void)stream_user_data;

  vec[0].base = nulldata;
  vec[0].len = NGHTTP3_MAX_VARINT + 1;

  return 1;
}

static nghttp3_ssize stream_data_almost_overflow_read_data(
  nghttp3_conn *conn, int64_t stream_id, nghttp3_vec *vec, size_t veccnt,
  uint32_t *pflags, void *user_data, void *stream_user_data) {
  (void)conn;
  (void)stream_id;
  (void)veccnt;
  (void)pflags;
  (void)user_data;
  (void)stream_user_data;

  vec[0].base = nulldata;
  vec[0].len = NGHTTP3_MAX_VARINT;

  return 1;
}
#endif /* SIZE_MAX > UINT32_MAX */

static int stop_sending(nghttp3_conn *conn, int64_t stream_id,
                        uint64_t app_error_code, void *user_data,
                        void *stream_user_data) {
  userdata *ud = user_data;

  (void)conn;
  (void)stream_user_data;

  ++ud->stop_sending_cb.ncalled;
  ud->stop_sending_cb.stream_id = stream_id;
  ud->stop_sending_cb.app_error_code = app_error_code;

  return 0;
}

static int reset_stream(nghttp3_conn *conn, int64_t stream_id,
                        uint64_t app_error_code, void *user_data,
                        void *stream_user_data) {
  userdata *ud = user_data;

  (void)conn;
  (void)stream_user_data;

  ++ud->reset_stream_cb.ncalled;
  ud->reset_stream_cb.stream_id = stream_id;
  ud->reset_stream_cb.app_error_code = app_error_code;

  return 0;
}

static int conn_shutdown(nghttp3_conn *conn, int64_t id, void *user_data) {
  userdata *ud = user_data;

  (void)conn;

  ++ud->shutdown_cb.ncalled;
  ud->shutdown_cb.id = id;

  return 0;
}

static int deferred_consume(nghttp3_conn *conn, int64_t stream_id,
                            size_t consumed, void *user_data,
                            void *stream_user_data) {
  userdata *ud = user_data;

  (void)conn;
  (void)stream_user_data;
  (void)stream_id;

  ud->deferred_consume_cb.consumed_total += consumed;

  return 0;
}

static int recv_settings(nghttp3_conn *conn, const nghttp3_settings *settings,
                         void *user_data) {
  userdata *ud = user_data;
  (void)conn;

  ++ud->recv_settings_cb.ncalled;
  ud->recv_settings_cb.settings = *settings;

  return 0;
}

typedef struct conn_options {
  nghttp3_callbacks *callbacks;
  nghttp3_settings *settings;
  int64_t control_stream_id;
  int64_t qenc_stream_id;
  int64_t qdec_stream_id;
  void *user_data;
} conn_options;

static void conn_options_clear(conn_options *opts) {
  memset(opts, 0, sizeof(*opts));
}

static void setup_conn_with_options(nghttp3_conn **pconn, int server,
                                    conn_options opts) {
  const nghttp3_mem *mem = nghttp3_mem_default();
  nghttp3_callbacks callbacks;
  nghttp3_settings settings;
  int rv;

  if (opts.callbacks == NULL) {
    memset(&callbacks, 0, sizeof(callbacks));
    opts.callbacks = &callbacks;
  }

  if (opts.settings == NULL) {
    nghttp3_settings_default(&settings);
    opts.settings = &settings;
  }

  if (server) {
    rv = nghttp3_conn_server_new(pconn, opts.callbacks, opts.settings, mem,
                                 opts.user_data);
  } else {
    rv = nghttp3_conn_client_new(pconn, opts.callbacks, opts.settings, mem,
                                 opts.user_data);
  }

  assert_int(0, ==, rv);

  rv = nghttp3_conn_bind_control_stream(*pconn, opts.control_stream_id);

  assert_int(0, ==, rv);
  assert_not_null((*pconn)->tx.ctrl);
  assert_uint64(NGHTTP3_STREAM_TYPE_CONTROL, ==, (*pconn)->tx.ctrl->type);

  rv = nghttp3_conn_bind_qpack_streams(*pconn, opts.qenc_stream_id,
                                       opts.qdec_stream_id);

  assert_int(0, ==, rv);
  assert_not_null((*pconn)->tx.qenc);
  assert_uint64(NGHTTP3_STREAM_TYPE_QPACK_ENCODER, ==, (*pconn)->tx.qenc->type);
  assert_not_null((*pconn)->tx.qdec);
  assert_uint64(NGHTTP3_STREAM_TYPE_QPACK_DECODER, ==, (*pconn)->tx.qdec->type);
}

static void setup_default_client_with_options(nghttp3_conn **pconn,
                                              conn_options opts) {
  if (opts.control_stream_id == 0) {
    opts.control_stream_id = 2;
  }

  if (opts.qenc_stream_id == 0) {
    opts.qenc_stream_id = 6;
  }

  if (opts.qdec_stream_id == 0) {
    opts.qdec_stream_id = 10;
  }

  setup_conn_with_options(pconn, 0, opts);
}

static void setup_default_client(nghttp3_conn **pconn) {
  conn_options opts = {0};

  setup_default_client_with_options(pconn, opts);
}

static void setup_default_server_with_options(nghttp3_conn **pconn,
                                              conn_options opts) {
  if (opts.control_stream_id == 0) {
    opts.control_stream_id = 3;
  }

  if (opts.qenc_stream_id == 0) {
    opts.qenc_stream_id = 7;
  }

  if (opts.qdec_stream_id == 0) {
    opts.qdec_stream_id = 11;
  }

  setup_conn_with_options(pconn, 1, opts);
}

static void setup_default_server(nghttp3_conn **pconn) {
  conn_options opts = {0};

  setup_default_server_with_options(pconn, opts);
}

static void conn_write_initial_streams(nghttp3_conn *conn) {
  nghttp3_vec vec[256];
  nghttp3_ssize sveccnt;
  int64_t stream_id;
  size_t len;
  int fin;
  int rv;

  /* control stream */
  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  assert_ptrdiff(1, ==, sveccnt);
  assert_int64(conn->tx.ctrl->node.id, ==, stream_id);
  assert_false(fin);

  len = (size_t)nghttp3_vec_len(vec, (size_t)sveccnt);
  rv = nghttp3_conn_add_write_offset(conn, stream_id, len);

  assert_int(0, ==, rv);

  rv = nghttp3_conn_add_ack_offset(conn, stream_id, len);

  assert_int(0, ==, rv);
  assert_uint64(len, ==, conn->tx.ctrl->ack_offset);

  /* QPACK decoder stream */
  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  assert_ptrdiff(1, ==, sveccnt);
  assert_int64(conn->tx.qdec->node.id, ==, stream_id);
  assert_false(fin);

  len = (size_t)nghttp3_vec_len(vec, (size_t)sveccnt);
  rv = nghttp3_conn_add_write_offset(conn, stream_id, len);

  assert_int(0, ==, rv);

  rv = nghttp3_conn_add_ack_offset(conn, stream_id, len);

  assert_int(0, ==, rv);
  assert_uint64(len, ==, conn->tx.qdec->ack_offset);

  /* QPACK encoder stream */
  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  assert_ptrdiff(1, ==, sveccnt);
  assert_int64(conn->tx.qenc->node.id, ==, stream_id);
  assert_false(fin);

  len = (size_t)nghttp3_vec_len(vec, (size_t)sveccnt);
  rv = nghttp3_conn_add_write_offset(conn, stream_id, len);

  assert_int(0, ==, rv);

  rv = nghttp3_conn_add_ack_offset(conn, stream_id, len);

  assert_int(0, ==, rv);
  assert_uint64(len, ==, conn->tx.qenc->ack_offset);

  /* Assert that client does not write more. */
  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  assert_ptrdiff(0, ==, sveccnt);
}

static void conn_read_control_stream(nghttp3_conn *conn, int64_t stream_id,
                                     nghttp3_frame *fr) {
  uint8_t rawbuf[1024];
  nghttp3_buf buf;
  nghttp3_ssize nconsumed;

  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  nghttp3_write_frame(&buf, fr);

  nconsumed = nghttp3_conn_read_stream(conn, stream_id, buf.pos,
                                       nghttp3_buf_len(&buf), /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, nconsumed);
}

void test_nghttp3_conn_read_control(void) {
  nghttp3_conn *conn;
  nghttp3_callbacks callbacks = {
    .recv_settings = recv_settings,
  };
  uint8_t rawbuf[2048];
  nghttp3_buf buf;
  struct {
    nghttp3_frame_settings settings;
    nghttp3_settings_entry iv[15];
  } fr;
  nghttp3_ssize nconsumed;
  nghttp3_settings_entry *iv;
  size_t i;
  nghttp3_stream *stream;
  userdata ud;
  conn_options opts;

  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  fr.settings.hd.type = NGHTTP3_FRAME_SETTINGS;
  iv = fr.settings.iv;
  iv[0].id = NGHTTP3_SETTINGS_ID_MAX_FIELD_SECTION_SIZE;
  iv[0].value = 65536;
  iv[1].id = 1000000009;
  iv[1].value = 1000000007;
  iv[2].id = NGHTTP3_SETTINGS_ID_QPACK_MAX_TABLE_CAPACITY;
  iv[2].value = 4096;
  iv[3].id = NGHTTP3_SETTINGS_ID_QPACK_BLOCKED_STREAMS;
  iv[3].value = 99;
  fr.settings.niv = 4;

  nghttp3_write_frame(&buf, (nghttp3_frame *)&fr);

  conn_options_clear(&opts);
  opts.callbacks = &callbacks;
  opts.user_data = &ud;

  setup_default_server_with_options(&conn, opts);

  memset(&ud, 0, sizeof(ud));
  nconsumed = nghttp3_conn_read_stream(conn, 2, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, nconsumed);
  assert_uint64(65536, ==, conn->remote.settings.max_field_section_size);
  assert_size(4096, ==, conn->remote.settings.qpack_max_dtable_capacity);
  assert_size(99, ==, conn->remote.settings.qpack_blocked_streams);
  assert_size(4096, ==, conn->qenc.ctx.hard_max_dtable_capacity);
  assert_size(4096, ==, conn->qenc.ctx.max_dtable_capacity);
  assert_size(99, ==, conn->qenc.ctx.max_blocked_streams);
  assert_size(1, ==, ud.recv_settings_cb.ncalled);
  assert_uint64(65536, ==, ud.recv_settings_cb.settings.max_field_section_size);
  assert_size(4096, ==, ud.recv_settings_cb.settings.qpack_max_dtable_capacity);
  assert_size(99, ==, ud.recv_settings_cb.settings.qpack_blocked_streams);

  nghttp3_conn_del(conn);

  /* Feed 1 byte at a time to verify that state machine works */
  conn_options_clear(&opts);
  opts.callbacks = &callbacks;
  opts.user_data = &ud;

  setup_default_server_with_options(&conn, opts);

  for (i = 0; i < nghttp3_buf_len(&buf); ++i) {
    nconsumed =
      nghttp3_conn_read_stream(conn, 2, buf.pos + i, 1, /* fin = */ 0);

    assert_ptrdiff(1, ==, nconsumed);
  }

  assert_uint64(65536, ==, conn->remote.settings.max_field_section_size);
  assert_size(4096, ==, conn->remote.settings.qpack_max_dtable_capacity);
  assert_size(99, ==, conn->remote.settings.qpack_blocked_streams);

  nghttp3_conn_del(conn);

  /* Receive empty SETTINGS */
  nghttp3_buf_reset(&buf);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  fr.settings.hd.type = NGHTTP3_FRAME_SETTINGS;
  fr.settings.niv = 0;

  nghttp3_write_frame(&buf, (nghttp3_frame *)&fr);

  conn_options_clear(&opts);
  opts.callbacks = &callbacks;
  opts.user_data = &ud;

  setup_default_server_with_options(&conn, opts);

  memset(&ud, 0, sizeof(ud));
  nconsumed = nghttp3_conn_read_stream(conn, 2, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, nconsumed);
  assert_size(1, ==, ud.recv_settings_cb.ncalled);
  assert_uint64(NGHTTP3_VARINT_MAX, ==,
                ud.recv_settings_cb.settings.max_field_section_size);
  assert_size(0, ==, ud.recv_settings_cb.settings.qpack_max_dtable_capacity);
  assert_size(0, ==, ud.recv_settings_cb.settings.qpack_blocked_streams);

  nghttp3_conn_del(conn);

  /* Receiver should enforce its own limits for QPACK parameters. */
  nghttp3_buf_reset(&buf);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  fr.settings.hd.type = NGHTTP3_FRAME_SETTINGS;
  iv = fr.settings.iv;
  iv[0].id = NGHTTP3_SETTINGS_ID_QPACK_MAX_TABLE_CAPACITY;
  iv[0].value = 4097;
  iv[1].id = NGHTTP3_SETTINGS_ID_QPACK_BLOCKED_STREAMS;
  iv[1].value = 101;
  fr.settings.niv = 2;

  nghttp3_write_frame(&buf, (nghttp3_frame *)&fr);

  conn_options_clear(&opts);
  opts.callbacks = &callbacks;
  opts.user_data = &ud;

  setup_default_server_with_options(&conn, opts);

  nconsumed = nghttp3_conn_read_stream(conn, 2, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, nconsumed);
  assert_size(4097, ==, conn->remote.settings.qpack_max_dtable_capacity);
  assert_size(101, ==, conn->remote.settings.qpack_blocked_streams);
  assert_size(4096, ==, conn->qenc.ctx.hard_max_dtable_capacity);
  assert_size(4096, ==, conn->qenc.ctx.max_dtable_capacity);
  assert_size(100, ==, conn->qenc.ctx.max_blocked_streams);

  nghttp3_conn_del(conn);

  /* Receiving multiple nonzero SETTINGS_QPACK_MAX_TABLE_CAPACITY is
     treated as error. */
  nghttp3_buf_reset(&buf);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  fr.settings.hd.type = NGHTTP3_FRAME_SETTINGS;
  iv = fr.settings.iv;
  iv[0].id = NGHTTP3_SETTINGS_ID_QPACK_MAX_TABLE_CAPACITY;
  iv[0].value = 4097;
  iv[1].id = NGHTTP3_SETTINGS_ID_QPACK_MAX_TABLE_CAPACITY;
  iv[1].value = 4097;
  fr.settings.niv = 2;

  nghttp3_write_frame(&buf, (nghttp3_frame *)&fr);

  conn_options_clear(&opts);
  opts.callbacks = &callbacks;
  opts.user_data = &ud;

  setup_default_server_with_options(&conn, opts);

  nconsumed = nghttp3_conn_read_stream(conn, 2, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff(NGHTTP3_ERR_H3_SETTINGS_ERROR, ==, nconsumed);

  nghttp3_conn_del(conn);

  /* Receiving multiple nonzero SETTINGS_QPACK_BLOCKED_STREAMS is
     treated as error. */
  nghttp3_buf_reset(&buf);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  fr.settings.hd.type = NGHTTP3_FRAME_SETTINGS;
  iv = fr.settings.iv;
  iv[0].id = NGHTTP3_SETTINGS_ID_QPACK_BLOCKED_STREAMS;
  iv[0].value = 1;
  iv[1].id = NGHTTP3_SETTINGS_ID_QPACK_BLOCKED_STREAMS;
  iv[1].value = 1;
  fr.settings.niv = 2;

  nghttp3_write_frame(&buf, (nghttp3_frame *)&fr);

  conn_options_clear(&opts);
  opts.callbacks = &callbacks;
  opts.user_data = &ud;

  setup_default_server_with_options(&conn, opts);

  nconsumed = nghttp3_conn_read_stream(conn, 2, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff(NGHTTP3_ERR_H3_SETTINGS_ERROR, ==, nconsumed);

  nghttp3_conn_del(conn);

  /* Receive ENABLE_CONNECT_PROTOCOL = 1 */
  nghttp3_buf_reset(&buf);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  fr.settings.hd.type = NGHTTP3_FRAME_SETTINGS;
  iv = fr.settings.iv;
  iv[0].id = NGHTTP3_SETTINGS_ID_ENABLE_CONNECT_PROTOCOL;
  iv[0].value = 1;
  fr.settings.niv = 1;

  nghttp3_write_frame(&buf, (nghttp3_frame *)&fr);

  conn_options_clear(&opts);
  opts.callbacks = &callbacks;
  opts.user_data = &ud;

  setup_default_server_with_options(&conn, opts);

  nconsumed = nghttp3_conn_read_stream(conn, 2, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, nconsumed);
  assert_uint8(1, ==, conn->remote.settings.enable_connect_protocol);

  nghttp3_conn_del(conn);

  /* Receiving ENABLE_CONNECT_PROTOCOL = 0 after seeing
     ENABLE_CONNECT_PROTOCOL = 1 */
  nghttp3_buf_reset(&buf);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  fr.settings.hd.type = NGHTTP3_FRAME_SETTINGS;
  iv = fr.settings.iv;
  iv[0].id = NGHTTP3_SETTINGS_ID_ENABLE_CONNECT_PROTOCOL;
  iv[0].value = 1;
  iv[1].id = NGHTTP3_SETTINGS_ID_ENABLE_CONNECT_PROTOCOL;
  iv[1].value = 0;
  fr.settings.niv = 2;

  nghttp3_write_frame(&buf, (nghttp3_frame *)&fr);

  conn_options_clear(&opts);
  opts.callbacks = &callbacks;
  opts.user_data = &ud;

  setup_default_server_with_options(&conn, opts);

  nconsumed = nghttp3_conn_read_stream(conn, 2, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff(NGHTTP3_ERR_H3_SETTINGS_ERROR, ==, nconsumed);

  nghttp3_conn_del(conn);

  /* Receive H3_DATAGRAM = 1 */
  nghttp3_buf_reset(&buf);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  fr.settings.hd.type = NGHTTP3_FRAME_SETTINGS;
  iv = fr.settings.iv;
  iv[0].id = NGHTTP3_SETTINGS_ID_H3_DATAGRAM;
  iv[0].value = 1;
  fr.settings.niv = 1;

  nghttp3_write_frame(&buf, (nghttp3_frame *)&fr);

  conn_options_clear(&opts);
  opts.callbacks = &callbacks;
  opts.user_data = &ud;

  setup_default_server_with_options(&conn, opts);

  nconsumed = nghttp3_conn_read_stream(conn, 2, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, nconsumed);
  assert_uint8(1, ==, conn->remote.settings.h3_datagram);

  nghttp3_conn_del(conn);

  /* Receive H3_DATAGRAM = 0 */
  nghttp3_buf_reset(&buf);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  fr.settings.hd.type = NGHTTP3_FRAME_SETTINGS;
  iv = fr.settings.iv;
  iv[0].id = NGHTTP3_SETTINGS_ID_H3_DATAGRAM;
  iv[0].value = 0;
  fr.settings.niv = 1;

  nghttp3_write_frame(&buf, (nghttp3_frame *)&fr);

  conn_options_clear(&opts);
  opts.callbacks = &callbacks;
  opts.user_data = &ud;

  setup_default_server_with_options(&conn, opts);

  nconsumed = nghttp3_conn_read_stream(conn, 2, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, nconsumed);
  assert_uint8(0, ==, conn->remote.settings.h3_datagram);

  nghttp3_conn_del(conn);

  /* Receive H3_DATAGRAM which is neither 0 nor 1 */
  nghttp3_buf_reset(&buf);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  fr.settings.hd.type = NGHTTP3_FRAME_SETTINGS;
  iv = fr.settings.iv;
  iv[0].id = NGHTTP3_SETTINGS_ID_H3_DATAGRAM;
  iv[0].value = 2;
  fr.settings.niv = 1;

  nghttp3_write_frame(&buf, (nghttp3_frame *)&fr);

  conn_options_clear(&opts);
  opts.callbacks = &callbacks;
  opts.user_data = &ud;

  setup_default_server_with_options(&conn, opts);

  nconsumed = nghttp3_conn_read_stream(conn, 2, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff(NGHTTP3_ERR_H3_SETTINGS_ERROR, ==, nconsumed);

  nghttp3_conn_del(conn);

  /* Receive SETTINGS in 1 byte at a time */
  nghttp3_buf_reset(&buf);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  fr.settings.hd.type = NGHTTP3_FRAME_SETTINGS;
  iv = fr.settings.iv;
  iv[0].id = NGHTTP3_SETTINGS_ID_ENABLE_CONNECT_PROTOCOL;
  iv[0].value = 1;
  iv[1].id = NGHTTP3_SETTINGS_ID_MAX_FIELD_SECTION_SIZE;
  iv[1].value = 4096;
  fr.settings.niv = 2;

  nghttp3_write_frame(&buf, (nghttp3_frame *)&fr);

  setup_default_server(&conn);

  for (i = 0; i < nghttp3_buf_len(&buf); ++i) {
    nconsumed = nghttp3_conn_read_stream(conn, 2, buf.pos + i, 1,
                                         /* fin = */ 0);

    assert_ptrdiff(1, ==, nconsumed);
  }

  stream = nghttp3_conn_find_stream(conn, 2);

  assert_int(NGHTTP3_CTRL_STREAM_STATE_FRAME_TYPE, ==, stream->rstate.state);

  nghttp3_conn_del(conn);

  /* Receive SETTINGS in 2 calls */
  nghttp3_buf_reset(&buf);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  fr.settings.hd.type = NGHTTP3_FRAME_SETTINGS;
  iv = fr.settings.iv;
  iv[0].id = NGHTTP3_SETTINGS_ID_ENABLE_CONNECT_PROTOCOL;
  iv[0].value = 1;
  iv[1].id = NGHTTP3_SETTINGS_ID_MAX_FIELD_SECTION_SIZE;
  iv[1].value = 4096;
  fr.settings.niv = 2;

  nghttp3_write_frame(&buf, (nghttp3_frame *)&fr);

  for (i = 1; i < nghttp3_buf_len(&buf) - 1; ++i) {
    setup_default_server_with_options(&conn, opts);

    nconsumed = nghttp3_conn_read_stream(conn, 2, buf.pos, i,
                                         /* fin = */ 0);

    assert_ptrdiff((nghttp3_ssize)i, ==, nconsumed);

    nconsumed =
      nghttp3_conn_read_stream(conn, 2, buf.pos + i, nghttp3_buf_len(&buf) - i,
                               /* fin = */ 0);

    assert_ptrdiff((nghttp3_ssize)(nghttp3_buf_len(&buf) - i), ==, nconsumed);

    stream = nghttp3_conn_find_stream(conn, 2);

    assert_int(NGHTTP3_CTRL_STREAM_STATE_FRAME_TYPE, ==, stream->rstate.state);

    nghttp3_conn_del(conn);
  }

  /* Receive SETTINGS frame that lacks value */
  nghttp3_buf_reset(&buf);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_FRAME_SETTINGS);
  buf.last = nghttp3_put_varint(buf.last,
                                (int64_t)nghttp3_put_varintlen(
                                  NGHTTP3_SETTINGS_ID_ENABLE_CONNECT_PROTOCOL));
  buf.last =
    nghttp3_put_varint(buf.last, NGHTTP3_SETTINGS_ID_ENABLE_CONNECT_PROTOCOL);

  setup_default_server(&conn);

  nconsumed = nghttp3_conn_read_stream(conn, 2, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff(NGHTTP3_ERR_H3_FRAME_ERROR, ==, nconsumed);

  nghttp3_conn_del(conn);

  /* Receive SETTINGS frame that lacks value in 2 calls */
  nghttp3_buf_reset(&buf);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_FRAME_SETTINGS);
  buf.last =
    nghttp3_put_varint(buf.last, (int64_t)nghttp3_put_varintlen(0xffff));
  buf.last = nghttp3_put_varint(buf.last, 0xffff);

  setup_default_server(&conn);

  nconsumed =
    nghttp3_conn_read_stream(conn, 2, buf.pos, nghttp3_buf_len(&buf) - 1,
                             /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)(nghttp3_buf_len(&buf) - 1), ==, nconsumed);

  nconsumed =
    nghttp3_conn_read_stream(conn, 2, buf.pos + nghttp3_buf_len(&buf) - 1, 1,
                             /* fin = */ 0);

  assert_ptrdiff(NGHTTP3_ERR_H3_FRAME_ERROR, ==, nconsumed);

  nghttp3_conn_del(conn);

  /* Receive a frame other than SETTINGS as a first frame */
  nghttp3_buf_reset(&buf);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  /* unknown frame */
  buf.last = nghttp3_put_varint(buf.last, 1000000007);
  /* and its length */
  buf.last = nghttp3_put_varint(buf.last, 1000000009);

  setup_default_server(&conn);

  nconsumed = nghttp3_conn_read_stream(conn, 2, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff(NGHTTP3_ERR_H3_MISSING_SETTINGS, ==, nconsumed);

  nghttp3_conn_del(conn);

  /* Receive SETTINGS frame more than once */
  nghttp3_buf_reset(&buf);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  fr.settings.hd.type = NGHTTP3_FRAME_SETTINGS;
  fr.settings.niv = 0;

  nghttp3_write_frame(&buf, (nghttp3_frame *)&fr);
  nghttp3_write_frame(&buf, (nghttp3_frame *)&fr);

  setup_default_server(&conn);

  nconsumed = nghttp3_conn_read_stream(conn, 2, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff(NGHTTP3_ERR_H3_FRAME_UNEXPECTED, ==, nconsumed);

  nghttp3_conn_del(conn);

  /* Receive an unknown frame */
  nghttp3_buf_reset(&buf);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  fr.settings.hd.type = NGHTTP3_FRAME_SETTINGS;
  fr.settings.niv = 0;

  nghttp3_write_frame(&buf, (nghttp3_frame *)&fr);

  /* unknown frame */
  buf.last = nghttp3_put_varint(buf.last, 1000000007);
  /* and its length */
  buf.last = nghttp3_put_varint(buf.last, 100);

  setup_default_server(&conn);

  nconsumed = nghttp3_conn_read_stream(conn, 2, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, nconsumed);

  stream = nghttp3_conn_find_stream(conn, 2);

  assert_int(NGHTTP3_CTRL_STREAM_STATE_IGN_FRAME, ==, stream->rstate.state);

  nghttp3_conn_del(conn);
}

void test_nghttp3_conn_write_control(void) {
  nghttp3_conn *conn;
  nghttp3_vec vec[256];
  nghttp3_ssize sveccnt;
  int64_t stream_id;
  int fin;
  nghttp3_settings settings;
  conn_options opts;
  nghttp3_ssize nread;
  union {
    nghttp3_frame fr;
    struct {
      nghttp3_frame_settings settings;
      nghttp3_settings pad[15];
    };
  } fr;

  setup_default_server(&conn);

  assert_not_null(conn->tx.ctrl);
  assert_uint64(NGHTTP3_STREAM_TYPE_CONTROL, ==, conn->tx.ctrl->type);

  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  assert_int64(3, ==, stream_id);
  assert_ptrdiff(1, ==, sveccnt);
  assert_size(1, <, vec[0].len);
  assert_uint8(NGHTTP3_STREAM_TYPE_CONTROL, ==, vec[0].base[0]);

  nghttp3_conn_del(conn);

  /* Enable H3 DATAGRAM and CONNECT protocol */
  nghttp3_settings_default(&settings);
  settings.h3_datagram = 1;
  settings.enable_connect_protocol = 1;

  conn_options_clear(&opts);
  opts.settings = &settings;

  setup_default_server_with_options(&conn, opts);

  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  assert_int64(3, ==, stream_id);
  assert_ptrdiff(1, ==, sveccnt);
  assert_size(1, <, vec[0].len);
  assert_uint8(NGHTTP3_STREAM_TYPE_CONTROL, ==, vec[0].base[0]);

  ++vec[0].base;
  --vec[0].len;

  nread = nghttp3_decode_settings_frame(&fr.fr.settings, vec, 1);

  assert_ptrdiff((nghttp3_ssize)vec[0].len, ==, nread);
  assert_size(5, ==, fr.fr.settings.niv);

  assert_uint64(NGHTTP3_SETTINGS_ID_MAX_FIELD_SECTION_SIZE, ==,
                fr.fr.settings.iv[0].id);
  assert_uint64(NGHTTP3_VARINT_MAX, ==, fr.fr.settings.iv[0].value);

  assert_uint64(NGHTTP3_SETTINGS_ID_QPACK_MAX_TABLE_CAPACITY, ==,
                fr.fr.settings.iv[1].id);
  assert_uint64(0, ==, fr.fr.settings.iv[1].value);

  assert_uint64(NGHTTP3_SETTINGS_ID_QPACK_BLOCKED_STREAMS, ==,
                fr.fr.settings.iv[2].id);
  assert_uint64(0, ==, fr.fr.settings.iv[2].value);

  assert_uint64(NGHTTP3_SETTINGS_ID_H3_DATAGRAM, ==, fr.fr.settings.iv[3].id);
  assert_uint64(1, ==, fr.fr.settings.iv[3].value);

  assert_uint64(NGHTTP3_SETTINGS_ID_ENABLE_CONNECT_PROTOCOL, ==,
                fr.fr.settings.iv[4].id);
  assert_uint64(1, ==, fr.fr.settings.iv[4].value);

  nghttp3_conn_del(conn);
}

void test_nghttp3_conn_submit_request(void) {
  nghttp3_conn *conn;
  nghttp3_callbacks callbacks = {
    .acked_stream_data = acked_stream_data,
  };
  nghttp3_vec vec[256];
  nghttp3_ssize sveccnt;
  int rv;
  int64_t stream_id;
  const nghttp3_nv large_nva[] = {
    MAKE_NV(":path", "/alpha/bravo/charlie/delta/echo/foxtrot/golf/hotel"),
    MAKE_NV(":authority", "example.com"),
    MAKE_NV(":scheme", "https"),
    MAKE_NV(":method", "GET"),
    MAKE_NV("priority", "u=0,i"),
    MAKE_NV("user-agent",
            "alpha=bravo; charlie=delta; echo=foxtrot; golf=hotel; "
            "india=juliett; kilo=lima; mike=november; oscar=papa; "
            "quebec=romeo; sierra=tango; uniform=vector; whiskey=xray; "
            "yankee=zulu"),
  };
  const nghttp3_nv trailer_nva[] = {
    MAKE_NV("digest", "foo"),
  };
  uint64_t len;
  size_t i;
  nghttp3_stream *stream;
  userdata ud = {0};
  nghttp3_data_reader dr;
  union {
    nghttp3_frame fr;
    struct {
      nghttp3_frame_settings settings;
      nghttp3_settings_entry iv[15];
    };
  } fr;
  nghttp3_settings_entry *iv;
  nghttp3_typed_buf *tbuf;
  int fin;
  size_t outq_idx;
  conn_options opts;

  ud.data.left = 2000;
  ud.data.step = 1200;

  conn_options_clear(&opts);
  opts.callbacks = &callbacks;
  opts.user_data = &ud;

  setup_default_client_with_options(&conn, opts);

  dr.read_data = step_read_data;
  rv = nghttp3_conn_submit_request(conn, 0, req_nva, nghttp3_arraylen(req_nva),
                                   &dr, NULL);

  assert_int(0, ==, rv);

  /* This will write control stream */
  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  assert_int64(2, ==, stream_id);
  assert_ptrdiff(1, ==, sveccnt);
  assert_uint64(0, <, nghttp3_vec_len(vec, (size_t)sveccnt));
  assert_size(1, ==, nghttp3_ringbuf_len(&conn->tx.qdec->outq));
  assert_size(0, ==, conn->tx.ctrl->outq_idx);

  tbuf = nghttp3_ringbuf_get(&conn->tx.ctrl->outq, 0);

  assert_size(0, ==, nghttp3_buf_offset(&tbuf->buf));

  rv = nghttp3_conn_add_write_offset(conn, 2, vec[0].len);

  assert_int(0, ==, rv);
  assert_size(1, ==, nghttp3_ringbuf_len(&conn->tx.ctrl->outq));
  assert_size(1, ==, conn->tx.ctrl->outq_idx);

  rv = nghttp3_conn_add_ack_offset(conn, 2, vec[0].len);

  assert_int(0, ==, rv);
  assert_size(0, ==, nghttp3_ringbuf_len(&conn->tx.ctrl->outq));
  assert_size(0, ==, conn->tx.ctrl->outq_idx);
  assert_uint64(vec[0].len, ==, conn->tx.ctrl->ack_offset);

  /* This will write QPACK decoder stream; just stream type */
  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  assert_int64(10, ==, stream_id);
  assert_ptrdiff(1, ==, sveccnt);
  assert_uint64(1, ==, nghttp3_vec_len(vec, (size_t)sveccnt));
  assert_size(1, ==, nghttp3_ringbuf_len(&conn->tx.qdec->outq));
  assert_size(0, ==, conn->tx.qdec->outq_idx);

  tbuf = nghttp3_ringbuf_get(&conn->tx.qdec->outq, 0);

  assert_size(0, ==, nghttp3_buf_offset(&tbuf->buf));

  /* Calling twice will return the same result */
  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  assert_int64(10, ==, stream_id);
  assert_ptrdiff(1, ==, sveccnt);

  rv = nghttp3_conn_add_write_offset(conn, 10, vec[0].len);

  assert_int(0, ==, rv);
  assert_size(1, ==, nghttp3_ringbuf_len(&conn->tx.qdec->outq));
  assert_size(1, ==, conn->tx.qdec->outq_idx);

  tbuf = nghttp3_ringbuf_get(&conn->tx.qdec->outq, 0);

  assert_size(vec[0].len, ==, nghttp3_buf_offset(&tbuf->buf));

  rv = nghttp3_conn_add_ack_offset(conn, 10, vec[0].len);

  assert_int(0, ==, rv);
  assert_size(0, ==, nghttp3_ringbuf_len(&conn->tx.qdec->outq));
  assert_size(0, ==, conn->tx.qdec->outq_idx);
  assert_uint64(1, ==, conn->tx.qdec->ack_offset);

  /* This will write QPACK encoder stream; just stream type */
  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  assert_int64(6, ==, stream_id);
  assert_ptrdiff(1, ==, sveccnt);

  rv = nghttp3_conn_add_write_offset(conn, 6, vec[0].len);

  assert_int(0, ==, rv);

  rv = nghttp3_conn_add_ack_offset(conn, 6, vec[0].len);

  assert_int(0, ==, rv);

  /* This will write request stream */
  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  assert_int64(0, ==, stream_id);
  assert_ptrdiff(2, ==, sveccnt);

  len = nghttp3_vec_len(vec, (size_t)sveccnt);
  for (i = 0; i < len; ++i) {
    rv = nghttp3_conn_add_write_offset(conn, 0, 1);

    assert_int(0, ==, rv);

    rv = nghttp3_conn_add_ack_offset(conn, 0, 1);

    assert_int(0, ==, rv);
  }

  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  assert_int64(0, ==, stream_id);
  assert_ptrdiff(2, ==, sveccnt);

  len = nghttp3_vec_len(vec, (size_t)sveccnt);

  for (i = 0; i < len; ++i) {
    rv = nghttp3_conn_add_write_offset(conn, 0, 1);

    assert_int(0, ==, rv);

    rv = nghttp3_conn_add_ack_offset(conn, 0, 1);

    assert_int(0, ==, rv);
  }

  stream = nghttp3_conn_find_stream(conn, 0);

  assert_size(0, ==, nghttp3_ringbuf_len(&stream->outq));
  assert_size(0, ==, nghttp3_ringbuf_len(&stream->chunks));
  assert_size(0, ==, stream->outq_idx);
  assert_uint64(2023, ==, stream->ack_offset);
  assert_uint64(2000, ==, ud.ack.acc);

  nghttp3_conn_del(conn);

  /* QPACK request buffer exceeds NGHTTP3_STREAM_MAX_COPY_THRES */
  setup_default_client(&conn);
  conn_write_initial_streams(conn);

  rv = nghttp3_conn_submit_request(conn, 0, large_nva,
                                   nghttp3_arraylen(large_nva), NULL, NULL);

  assert_int(0, ==, rv);

  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  assert_int64(0, ==, stream_id);
  /* Extra vector */
  assert_ptrdiff(2, ==, sveccnt);

  rv = nghttp3_conn_add_write_offset(
    conn, stream_id, (size_t)nghttp3_vec_len(vec, (size_t)sveccnt));

  assert_int(0, ==, rv);

  rv = nghttp3_conn_add_ack_offset(conn, stream_id,
                                   nghttp3_vec_len(vec, (size_t)sveccnt));

  assert_int(0, ==, rv);

  nghttp3_conn_del(conn);

  /* QPACK encoder buffer exceeds NGHTTP3_STREAM_MAX_COPY_THRES */
  setup_default_client(&conn);
  conn_write_initial_streams(conn);

  fr.settings.hd.type = NGHTTP3_FRAME_SETTINGS;
  fr.settings.niv = 1;
  iv = fr.settings.iv;
  iv[0].id = NGHTTP3_SETTINGS_ID_QPACK_MAX_TABLE_CAPACITY;
  iv[0].value = 4096;

  conn_read_control_stream(conn, 3, &fr.fr);

  rv = nghttp3_conn_submit_request(conn, 0, large_nva,
                                   nghttp3_arraylen(large_nva), NULL, NULL);

  assert_int(0, ==, rv);

  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  assert_int64(conn->tx.qenc->node.id, ==, stream_id);
  assert_ptrdiff(1, ==, sveccnt);

  tbuf = nghttp3_ringbuf_get(&conn->tx.qenc->outq,
                             nghttp3_ringbuf_len(&conn->tx.qenc->outq) - 1);

  assert_enum(nghttp3_buf_type, NGHTTP3_BUF_TYPE_PRIVATE, ==, tbuf->type);

  rv = nghttp3_conn_add_write_offset(
    conn, stream_id, (size_t)nghttp3_vec_len(vec, (size_t)sveccnt));

  assert_int(0, ==, rv);

  rv = nghttp3_conn_add_ack_offset(conn, stream_id,
                                   nghttp3_vec_len(vec, (size_t)sveccnt));

  assert_int(0, ==, rv);

  nghttp3_conn_del(conn);

  /* Extend the last shared buffer after it is written out */
  setup_default_client(&conn);
  conn_write_initial_streams(conn);

  dr.read_data = empty_read_data;
  rv = nghttp3_conn_submit_request(conn, 0, req_nva, nghttp3_arraylen(req_nva),
                                   &dr, NULL);

  assert_int(0, ==, rv);

  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  assert_int64(0, ==, stream_id);
  assert_ptrdiff(1, ==, sveccnt);

  rv = nghttp3_conn_add_write_offset(
    conn, stream_id, (size_t)nghttp3_vec_len(vec, (size_t)sveccnt));

  assert_int(0, ==, rv);

  stream = nghttp3_conn_find_stream(conn, stream_id);
  outq_idx = stream->outq_idx;

  rv = nghttp3_conn_submit_trailers(conn, 0, trailer_nva,
                                    nghttp3_arraylen(trailer_nva));

  assert_int(0, ==, rv);

  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  assert_int64(0, ==, stream_id);
  assert_ptrdiff(1, ==, sveccnt);
  assert_size(outq_idx - 1, ==, stream->outq_idx);

  nghttp3_conn_del(conn);

  /* Make sure that just sending fin works. */
  conn_options_clear(&opts);
  opts.user_data = &ud;

  setup_default_client_with_options(&conn, opts);
  conn_write_initial_streams(conn);

  dr.read_data = block_then_step_read_data;
  rv = nghttp3_conn_submit_request(conn, 0, req_nva, nghttp3_arraylen(req_nva),
                                   &dr, NULL);

  assert_int(0, ==, rv);

  ud.data.nblock = 1;
  ud.data.left = 0;
  ud.data.step = 0;

  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  assert_int64(0, ==, stream_id);
  assert_ptrdiff(1, ==, sveccnt);
  assert_false(fin);

  len = nghttp3_vec_len(vec, (size_t)sveccnt);

  rv = nghttp3_conn_add_write_offset(conn, stream_id, (size_t)len);

  assert_int(0, ==, rv);

  rv = nghttp3_conn_resume_stream(conn, stream_id);

  assert_int(0, ==, rv);

  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  assert_int64(0, ==, stream_id);
  assert_ptrdiff(0, ==, sveccnt);
  assert_true(fin);

  /* This should not acknowledge fin which has not yet been handed out
     to network. */
  rv = nghttp3_conn_add_ack_offset(conn, stream_id, len);

  assert_int(0, ==, rv);

  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  assert_int64(0, ==, stream_id);
  assert_ptrdiff(0, ==, sveccnt);
  assert_true(fin);

  rv = nghttp3_conn_add_write_offset(conn, stream_id, 0);

  assert_int(0, ==, rv);

  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  assert_int64(-1, ==, stream_id);
  assert_ptrdiff(0, ==, sveccnt);

  rv = nghttp3_conn_add_ack_offset(conn, stream_id, 0);

  assert_int(0, ==, rv);

  nghttp3_conn_del(conn);
}

void test_nghttp3_conn_http_request(void) {
  nghttp3_conn *cl, *sv;
  nghttp3_callbacks callbacks = {
    .begin_headers = begin_headers,
    .recv_header = recv_header,
    .end_headers = end_headers,
  };
  nghttp3_settings settings;
  nghttp3_vec vec[256];
  nghttp3_ssize sveccnt;
  nghttp3_ssize sconsumed;
  int rv;
  int64_t stream_id;
  const nghttp3_nv respnva[] = {
    MAKE_NV(":status", "200"),
    MAKE_NV("server", "nghttp3"),
    MAKE_NV("content-length", "1999"),
  };
  nghttp3_data_reader dr;
  int fin;
  userdata clud = {0}, svud = {0};
  size_t i;
  size_t nconsumed;
  size_t nread;
  conn_options opts;

  nghttp3_settings_default(&settings);

  settings.qpack_max_dtable_capacity = 4096;
  settings.qpack_blocked_streams = 100;

  clud.data.left = 2000;
  clud.data.step = 1200;

  svud.data.left = 1999;
  svud.data.step = 1000;

  conn_options_clear(&opts);
  opts.callbacks = &callbacks;
  opts.settings = &settings;

  opts.user_data = &clud;

  setup_default_client_with_options(&cl, opts);

  opts.user_data = &svud;

  setup_default_server_with_options(&sv, opts);

  dr.read_data = step_read_data;
  rv = nghttp3_conn_submit_request(cl, 0, req_nva, nghttp3_arraylen(req_nva),
                                   &dr, NULL);

  assert_int(0, ==, rv);

  nread = 0;
  nconsumed = 0;

  for (;;) {
    sveccnt = nghttp3_conn_writev_stream(cl, &stream_id, &fin, vec,
                                         nghttp3_arraylen(vec));

    assert_ptrdiff(0, <=, sveccnt);

    if (sveccnt <= 0) {
      break;
    }

    rv = nghttp3_conn_add_write_offset(
      cl, stream_id, (size_t)nghttp3_vec_len(vec, (size_t)sveccnt));

    assert_int(0, ==, rv);

    for (i = 0; i < (size_t)sveccnt; ++i) {
      sconsumed =
        nghttp3_conn_read_stream(sv, stream_id, vec[i].base, vec[i].len,
                                 fin && i == (size_t)sveccnt - 1);
      assert_ptrdiff(0, <=, sconsumed);

      nread += vec[i].len;
      nconsumed += (size_t)sconsumed;
    }

    rv = nghttp3_conn_add_ack_offset(cl, stream_id,
                                     nghttp3_vec_len(vec, (size_t)sveccnt));

    assert_int(0, ==, rv);
  }

  assert_size(nread, ==, nconsumed + 2000);

  rv = nghttp3_conn_submit_response(sv, 0, respnva, nghttp3_arraylen(respnva),
                                    &dr);

  assert_int(0, ==, rv);

  nread = 0;
  nconsumed = 0;

  for (;;) {
    sveccnt = nghttp3_conn_writev_stream(sv, &stream_id, &fin, vec,
                                         nghttp3_arraylen(vec));

    assert_ptrdiff(0, <=, sveccnt);

    if (sveccnt <= 0) {
      break;
    }

    rv = nghttp3_conn_add_write_offset(
      sv, stream_id, (size_t)nghttp3_vec_len(vec, (size_t)sveccnt));

    assert_int(0, ==, rv);

    for (i = 0; i < (size_t)sveccnt; ++i) {
      sconsumed =
        nghttp3_conn_read_stream(cl, stream_id, vec[i].base, vec[i].len,
                                 fin && i == (size_t)sveccnt - 1);
      assert_ptrdiff(0, <=, sconsumed);

      nread += vec[i].len;
      nconsumed += (size_t)sconsumed;
    }

    rv = nghttp3_conn_add_ack_offset(sv, stream_id,
                                     nghttp3_vec_len(vec, (size_t)sveccnt));

    assert_int(0, ==, rv);
  }

  assert_size(nread, ==, nconsumed + 1999);

  nghttp3_conn_del(sv);
  nghttp3_conn_del(cl);
}

static void check_http_header(const nghttp3_nv *nva, size_t nvlen, int request,
                              int want_lib_error) {
  uint8_t rawbuf[4096];
  nghttp3_buf buf;
  nghttp3_frame_headers fr;
  nghttp3_conn *conn;
  nghttp3_settings settings;
  const nghttp3_mem *mem = nghttp3_mem_default();
  nghttp3_ssize sconsumed;
  nghttp3_qpack_encoder qenc;
  nghttp3_stream *stream;
  conn_options opts;

  nghttp3_settings_default(&settings);
  settings.enable_connect_protocol = 1;

  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)nva;
  fr.nvlen = nvlen;

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  conn_options_clear(&opts);
  opts.settings = &settings;

  if (request) {
    setup_default_server_with_options(&conn, opts);
    nghttp3_conn_set_max_client_streams_bidi(conn, 1);
  } else {
    setup_default_client_with_options(&conn, opts);
    nghttp3_conn_create_stream(conn, &stream, 0);
    stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;
  }

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  if (want_lib_error) {
    if (want_lib_error == NGHTTP3_ERR_MALFORMED_HTTP_HEADER) {
      assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, sconsumed);

      stream = nghttp3_conn_find_stream(conn, 0);

      assert_true(stream->flags & NGHTTP3_STREAM_FLAG_HTTP_ERROR);
    } else {
      assert_ptrdiff(want_lib_error, ==, sconsumed);
    }
  } else {
    assert_ptrdiff(0, <, sconsumed);
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
  /* response header has te header field that contains invalid
     value. */
  const nghttp3_nv invalidte_resnv[] = {
    MAKE_NV(":status", "200"),
    MAKE_NV("te", "trailer2"),
  };
  /* response header has te header field that contains TRAiLERS. */
  const nghttp3_nv te_resnv[] = {
    MAKE_NV(":status", "200"),
    MAKE_NV("te", "TRAiLERS"),
  };
  /* response header has a bad header value. */
  const nghttp3_nv badvalue_resnv[] = {
    MAKE_NV(":status", "200"),
    MAKE_NV("foo", "\x7f"),
  };
  /* response header has empty header name followed by a pseudo
     header. */
  const nghttp3_nv emptynamepseudo_resnv[] = {
    MAKE_NV("", "foo"),
    MAKE_NV(":status", "200"),
  };
  /* response header contains a upper-cased header name. */
  const nghttp3_nv upcasename_resnv[] = {
    MAKE_NV(":status", "200"),
    MAKE_NV("Cookie", "foo=bar"),
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
  /* Ignore content-length in 1xx response. */
  check_http_resp_header(cl1xx_resnv, nghttp3_arraylen(cl1xx_resnv), 0);
  /* This is allowed to work with widely used services. */
  check_http_resp_header(cl204_resnv, nghttp3_arraylen(cl204_resnv), 0);
  check_http_resp_header(clnonzero204_resnv,
                         nghttp3_arraylen(clnonzero204_resnv),
                         NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
  check_http_resp_header(status101_resnv, nghttp3_arraylen(status101_resnv),
                         NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
  check_http_resp_header(invalidte_resnv, nghttp3_arraylen(invalidte_resnv),
                         NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
  check_http_resp_header(te_resnv, nghttp3_arraylen(te_resnv), 0);
  check_http_resp_header(badvalue_resnv, nghttp3_arraylen(badvalue_resnv), 0);
  check_http_resp_header(emptynamepseudo_resnv,
                         nghttp3_arraylen(emptynamepseudo_resnv),
                         NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
  check_http_resp_header(upcasename_resnv, nghttp3_arraylen(upcasename_resnv),
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
  /* request header has content-length that is empty string */
  const nghttp3_nv emptycl_reqnv[] = {
    MAKE_NV(":scheme", "https"),        MAKE_NV(":method", "POST"),
    MAKE_NV(":authority", "localhost"), MAKE_NV(":path", "/"),
    MAKE_NV("content-length", ""),
  };
  /* request header has content-length that is greater than
     NGHTTP3_MAX_VARINT */
  const nghttp3_nv toolargecl_reqnv[] = {
    MAKE_NV(":scheme", "https"),
    MAKE_NV(":method", "POST"),
    MAKE_NV(":authority", "localhost"),
    MAKE_NV(":path", "/"),
    MAKE_NV("content-length", "4611686018427387904"),
  };
  /* request header has content-length that is much greater than
     NGHTTP3_MAX_VARINT */
  const nghttp3_nv fartoolargecl_reqnv[] = {
    MAKE_NV(":scheme", "https"),
    MAKE_NV(":method", "POST"),
    MAKE_NV(":authority", "localhost"),
    MAKE_NV(":path", "/"),
    MAKE_NV("content-length", "5611686018427387904"),
  };
  /* request header has content-length that is equal to
     NGHTTP3_MAX_VARINT */
  const nghttp3_nv largestcl_reqnv[] = {
    MAKE_NV(":scheme", "https"),
    MAKE_NV(":method", "POST"),
    MAKE_NV(":authority", "localhost"),
    MAKE_NV(":path", "/"),
    MAKE_NV("content-length", "4611686018427387903"),
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
  /* scheme is an empty string. */
  const nghttp3_nv emptyscheme_reqnv[] = {
    MAKE_NV(":scheme", ""),
    MAKE_NV(":path", "/"),
    MAKE_NV(":method", "GET"),
    MAKE_NV(":authority", "localhost"),
  };
  /* scheme contains a string that starts with a character that is not
     in [a-zA-Z]. */
  const nghttp3_nv badprefixscheme_reqnv[] = {
    MAKE_NV(":scheme", "@"),
    MAKE_NV(":path", "/"),
    MAKE_NV(":method", "GET"),
    MAKE_NV(":authority", "localhost"),
  };
  /* scheme contains a bad character. */
  const nghttp3_nv badcharscheme_reqnv[] = {
    MAKE_NV(":scheme", "http*"),
    MAKE_NV(":path", "/"),
    MAKE_NV(":method", "GET"),
    MAKE_NV(":authority", "localhost"),
  };
  /* scheme contains all allowed characters. */
  const nghttp3_nv allcharscheme_reqnv[] = {
    MAKE_NV(
      ":scheme",
      "aabcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+-."),
    MAKE_NV(":path", "/"),
    MAKE_NV(":method", "GET"),
    MAKE_NV(":authority", "localhost"),
  };
  /* method is an empty string. */
  const nghttp3_nv emptymethod_reqnv[] = {
    MAKE_NV(":scheme", "https"),
    MAKE_NV(":path", "/"),
    MAKE_NV(":method", ""),
    MAKE_NV(":authority", "localhost"),
  };
  /* method contains a bad character. */
  const nghttp3_nv badcharmethod_reqnv[] = {
    MAKE_NV(":scheme", "https"),
    MAKE_NV(":path", "/"),
    MAKE_NV(":method", "GET\xb2"),
    MAKE_NV(":authority", "localhost"),
  };
  /* empty :path for https URI. */
  const nghttp3_nv emptyhttpspath_reqnv[] = {
    MAKE_NV(":scheme", "https"),
    MAKE_NV(":path", ""),
    MAKE_NV(":method", "GET"),
    MAKE_NV(":authority", "localhost"),
  };
  /* empty :path for http URI. */
  const nghttp3_nv emptyhttppath_reqnv[] = {
    MAKE_NV(":scheme", "http"),
    MAKE_NV(":path", ""),
    MAKE_NV(":method", "GET"),
    MAKE_NV(":authority", "localhost"),
  };
  /* empty :path for non-https/http URI. */
  const nghttp3_nv emptypath_reqnv[] = {
    MAKE_NV(":scheme", "something"),
    MAKE_NV(":path", ""),
    MAKE_NV(":method", "GET"),
    MAKE_NV(":authority", "localhost"),
  };
  /* :path contains a bad character. */
  const nghttp3_nv badcharpath_reqnv[] = {
    MAKE_NV(":scheme", "https"),
    MAKE_NV(":path", "/\x01"),
    MAKE_NV(":method", "GET"),
    MAKE_NV(":authority", "localhost"),
  };
  /* :path contains all allowed characters. */
  const nghttp3_nv allowedcharpath_reqnv[] = {
    MAKE_NV(":scheme", "https"),
    MAKE_NV(":path", "/ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123"
                     "456789-._~!$&'()*+,;=?%:@"),
    MAKE_NV(":method", "GET"),
    MAKE_NV(":authority", "localhost"),
  };
  /* HEAD method is used. */
  const nghttp3_nv headmethod_reqnv[] = {
    MAKE_NV(":scheme", "https"),
    MAKE_NV(":path", "/"),
    MAKE_NV(":method", "HEAD"),
    MAKE_NV(":authority", "localhost"),
  };
  /* :protocol is given twice. */
  const nghttp3_nv dupproto_reqnv[] = {
    MAKE_NV(":scheme", "https"),       MAKE_NV(":path", "/"),
    MAKE_NV(":method", "CONNECT"),     MAKE_NV(":authority", "localhost"),
    MAKE_NV(":protocol", "websocket"), MAKE_NV(":protocol", "websocket"),
  };
  /* host contains a bad character. */
  const nghttp3_nv badcharhost_reqnv[] = {
    MAKE_NV(":scheme", "https"),
    MAKE_NV(":path", "/"),
    MAKE_NV(":method", "HEAD"),
    MAKE_NV("host", "localhost\x99"),
  };
  /* host is given twice. */
  const nghttp3_nv duphost_reqnv[] = {
    MAKE_NV(":scheme", "https"),  MAKE_NV(":path", "/"),
    MAKE_NV(":method", "HEAD"),   MAKE_NV("host", "localhost"),
    MAKE_NV("host", "localhost"),
  };
  /* request header has te header field that contains invalid
     value. */
  const nghttp3_nv invalidte_reqnv[] = {
    MAKE_NV(":scheme", "https"), MAKE_NV(":path", "/"),
    MAKE_NV(":method", "GET"),   MAKE_NV(":authority", "localhost"),
    MAKE_NV("te", "trailer2"),
  };
  /* request header has te header field that contains TRAiLERS. */
  const nghttp3_nv te_reqnv[] = {
    MAKE_NV(":scheme", "https"), MAKE_NV(":path", "/"),
    MAKE_NV(":method", "GET"),   MAKE_NV(":authority", "localhost"),
    MAKE_NV("te", "TRAiLERS"),
  };
  /* priority header has a bad character. */
  const nghttp3_nv badcharpriority_reqnv[] = {
    MAKE_NV(":scheme", "https"), MAKE_NV(":path", "/"),
    MAKE_NV(":method", "GET"),   MAKE_NV(":authority", "localhost"),
    MAKE_NV("priority", "\x7f"),
  };
  /* priority header is followed by bad priority header. */
  const nghttp3_nv dupbadcharpriority_reqnv[] = {
    MAKE_NV(":scheme", "https"), MAKE_NV(":path", "/"),
    MAKE_NV(":method", "GET"),   MAKE_NV(":authority", "localhost"),
    MAKE_NV("priority", "\x7f"), MAKE_NV("priority", "i"),
  };
  /* request header has :status header. */
  const nghttp3_nv unknownpseudohd_reqnv[] = {
    MAKE_NV(":scheme", "https"),     MAKE_NV(":path", "/"),
    MAKE_NV(":method", "GET"),       MAKE_NV(":authority", "localhost"),
    MAKE_NV(":status", "localhost"),
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
  check_http_req_header(emptycl_reqnv, nghttp3_arraylen(emptycl_reqnv),
                        NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
  check_http_req_header(largestcl_reqnv, nghttp3_arraylen(largestcl_reqnv), 0);
  check_http_req_header(toolargecl_reqnv, nghttp3_arraylen(toolargecl_reqnv),
                        NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
  check_http_req_header(fartoolargecl_reqnv,
                        nghttp3_arraylen(fartoolargecl_reqnv),
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
                        0);
  check_http_req_header(invalidvalue_reqnv,
                        nghttp3_arraylen(invalidvalue_reqnv), 0);
  check_http_req_header(connectproto_reqnv,
                        nghttp3_arraylen(connectproto_reqnv), 0);
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
  check_http_req_header(emptyscheme_reqnv, nghttp3_arraylen(emptyscheme_reqnv),
                        NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
  check_http_req_header(badprefixscheme_reqnv,
                        nghttp3_arraylen(badprefixscheme_reqnv),
                        NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
  check_http_req_header(badcharscheme_reqnv,
                        nghttp3_arraylen(badcharscheme_reqnv),
                        NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
  check_http_req_header(allcharscheme_reqnv,
                        nghttp3_arraylen(allcharscheme_reqnv), 0);
  check_http_req_header(emptymethod_reqnv, nghttp3_arraylen(emptymethod_reqnv),
                        NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
  check_http_req_header(badcharmethod_reqnv,
                        nghttp3_arraylen(badcharmethod_reqnv),
                        NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
  check_http_req_header(emptyhttpspath_reqnv,
                        nghttp3_arraylen(emptyhttpspath_reqnv),
                        NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
  check_http_req_header(emptyhttppath_reqnv,
                        nghttp3_arraylen(emptyhttppath_reqnv),
                        NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
  check_http_req_header(emptypath_reqnv, nghttp3_arraylen(emptypath_reqnv), 0);
  check_http_req_header(badcharpath_reqnv, nghttp3_arraylen(badcharpath_reqnv),
                        NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
  check_http_req_header(allowedcharpath_reqnv,
                        nghttp3_arraylen(allowedcharpath_reqnv), 0);
  check_http_req_header(headmethod_reqnv, nghttp3_arraylen(headmethod_reqnv),
                        0);
  check_http_req_header(dupproto_reqnv, nghttp3_arraylen(dupproto_reqnv),
                        NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
  check_http_req_header(badcharhost_reqnv, nghttp3_arraylen(badcharhost_reqnv),
                        NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
  check_http_req_header(duphost_reqnv, nghttp3_arraylen(duphost_reqnv),
                        NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
  check_http_req_header(invalidte_reqnv, nghttp3_arraylen(invalidte_reqnv),
                        NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
  check_http_req_header(te_reqnv, nghttp3_arraylen(te_reqnv), 0);
  check_http_req_header(badcharpriority_reqnv,
                        nghttp3_arraylen(badcharpriority_reqnv), 0);
  check_http_req_header(dupbadcharpriority_reqnv,
                        nghttp3_arraylen(dupbadcharpriority_reqnv), 0);
  check_http_req_header(unknownpseudohd_reqnv,
                        nghttp3_arraylen(unknownpseudohd_reqnv),
                        NGHTTP3_ERR_MALFORMED_HTTP_HEADER);
}

void test_nghttp3_conn_http_content_length(void) {
  uint8_t rawbuf[4096];
  nghttp3_buf buf;
  nghttp3_frame_headers fr;
  nghttp3_conn *conn;
  const nghttp3_mem *mem = nghttp3_mem_default();
  nghttp3_ssize sconsumed;
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

  /* client */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)resnv;
  fr.nvlen = nghttp3_arraylen(resnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  setup_default_client(&conn);
  nghttp3_conn_create_stream(conn, &stream, 0);
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, sconsumed);
  assert_int64(9000000000LL, ==, stream->rx.http.content_length);
  assert_int32(200, ==, stream->rx.http.status_code);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* server */
  nghttp3_buf_reset(&buf);
  nghttp3_qpack_encoder_init(&qenc, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)reqnv;
  fr.nvlen = nghttp3_arraylen(reqnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  setup_default_server(&conn);
  nghttp3_conn_set_max_client_streams_bidi(conn, 1);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, sconsumed);

  stream = nghttp3_conn_find_stream(conn, 0);

  assert_int64(9000000000LL, ==, stream->rx.http.content_length);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);
}

void test_nghttp3_conn_http_content_length_mismatch(void) {
  uint8_t rawbuf[4096];
  nghttp3_buf buf;
  nghttp3_frame_headers fr;
  nghttp3_conn *conn;
  const nghttp3_mem *mem = nghttp3_mem_default();
  nghttp3_ssize sconsumed;
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

  /* content-length is 20, but no DATA is present and see fin */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)reqnv;
  fr.nvlen = nghttp3_arraylen(reqnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  setup_default_server(&conn);
  nghttp3_conn_set_max_client_streams_bidi(conn, 1);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 1);

  assert_ptrdiff(NGHTTP3_ERR_MALFORMED_HTTP_MESSAGING, ==, sconsumed);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* content-length is 20, but no DATA is present and stream is
     reset */
  nghttp3_buf_reset(&buf);
  nghttp3_qpack_encoder_init(&qenc, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)reqnv;
  fr.nvlen = nghttp3_arraylen(reqnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  setup_default_server(&conn);
  nghttp3_conn_set_max_client_streams_bidi(conn, 1);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, sconsumed);

  rv = nghttp3_conn_shutdown_stream_read(conn, 0);

  assert_int(0, ==, rv);

  rv = nghttp3_conn_close_stream(conn, 0, NGHTTP3_H3_NO_ERROR);

  assert_int(0, ==, rv);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* content-length is 20, but server receives 21 bytes DATA. */
  nghttp3_buf_reset(&buf);
  nghttp3_qpack_encoder_init(&qenc, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)reqnv;
  fr.nvlen = nghttp3_arraylen(reqnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);
  nghttp3_write_frame_data(&buf, 21);

  setup_default_server(&conn);
  nghttp3_conn_set_max_client_streams_bidi(conn, 1);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff(NGHTTP3_ERR_MALFORMED_HTTP_MESSAGING, ==, sconsumed);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* Check client side as well */

  /* content-length is 20, but no DATA is present and see fin */
  nghttp3_buf_reset(&buf);
  nghttp3_qpack_encoder_init(&qenc, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)resnv;
  fr.nvlen = nghttp3_arraylen(resnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  setup_default_client(&conn);
  nghttp3_conn_create_stream(conn, &stream, 0);
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 1);

  assert_ptrdiff(NGHTTP3_ERR_MALFORMED_HTTP_MESSAGING, ==, sconsumed);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* content-length is 20, but no DATA is present and stream is
     reset */
  nghttp3_buf_reset(&buf);
  nghttp3_qpack_encoder_init(&qenc, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)resnv;
  fr.nvlen = nghttp3_arraylen(resnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  setup_default_client(&conn);
  nghttp3_conn_create_stream(conn, &stream, 0);
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, sconsumed);

  rv = nghttp3_conn_shutdown_stream_read(conn, 0);

  assert_int(0, ==, rv);

  rv = nghttp3_conn_close_stream(conn, 0, NGHTTP3_H3_NO_ERROR);

  assert_int(0, ==, rv);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* content-length is 20, but server receives 21 bytes DATA. */
  nghttp3_buf_reset(&buf);
  nghttp3_qpack_encoder_init(&qenc, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)resnv;
  fr.nvlen = nghttp3_arraylen(resnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);
  nghttp3_write_frame_data(&buf, 21);

  setup_default_client(&conn);
  nghttp3_conn_create_stream(conn, &stream, 0);
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff(NGHTTP3_ERR_MALFORMED_HTTP_MESSAGING, ==, sconsumed);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);
}

void test_nghttp3_conn_http_non_final_response(void) {
  uint8_t rawbuf[4096];
  nghttp3_buf buf;
  nghttp3_frame_headers fr;
  nghttp3_conn *conn;
  const nghttp3_mem *mem = nghttp3_mem_default();
  nghttp3_ssize sconsumed;
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

  /* non-final followed by DATA is illegal.  */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)infonv;
  fr.nvlen = nghttp3_arraylen(infonv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);
  nghttp3_write_frame_data(&buf, 0);

  setup_default_client(&conn);
  nghttp3_conn_create_stream(conn, &stream, 0);
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff(NGHTTP3_ERR_H3_FRAME_UNEXPECTED, ==, sconsumed);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* 2 non-finals followed by final headers */
  nghttp3_buf_reset(&buf);
  nghttp3_qpack_encoder_init(&qenc, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)infonv;
  fr.nvlen = nghttp3_arraylen(infonv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);
  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)resnv;
  fr.nvlen = nghttp3_arraylen(resnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  setup_default_client(&conn);
  nghttp3_conn_create_stream(conn, &stream, 0);
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, sconsumed);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* non-finals followed by trailers; this trailer is treated as
     another non-final or final header fields.  Since it does not
     include mandatory header field, it is treated as error. */
  nghttp3_buf_reset(&buf);
  nghttp3_qpack_encoder_init(&qenc, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)infonv;
  fr.nvlen = nghttp3_arraylen(infonv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)trnv;
  fr.nvlen = nghttp3_arraylen(trnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  setup_default_client(&conn);
  nghttp3_conn_create_stream(conn, &stream, 0);
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, sconsumed);

  stream = nghttp3_conn_find_stream(conn, 0);

  assert_true(stream->flags & NGHTTP3_STREAM_FLAG_HTTP_ERROR);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);
}

void test_nghttp3_conn_http_trailers(void) {
  uint8_t rawbuf[4096];
  nghttp3_buf buf;
  nghttp3_frame_headers fr;
  nghttp3_conn *conn;
  const nghttp3_mem *mem = nghttp3_mem_default();
  nghttp3_ssize sconsumed;
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
  const nghttp3_nv trnv[] = {
    MAKE_NV("foo", "bar"),
  };
  const nghttp3_nv clnv[] = {
    MAKE_NV("content-length", "0"),
  };
  nghttp3_stream *stream;
  userdata ud;
  nghttp3_callbacks callbacks;
  conn_options opts;

  /* final response followed by trailers */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)resp_nva;
  fr.nvlen = nghttp3_arraylen(resp_nva);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)trnv;
  fr.nvlen = nghttp3_arraylen(trnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  setup_default_client(&conn);
  nghttp3_conn_create_stream(conn, &stream, 0);
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, sconsumed);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* trailers contain :status */
  nghttp3_buf_reset(&buf);
  nghttp3_qpack_encoder_init(&qenc, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)resp_nva;
  fr.nvlen = nghttp3_arraylen(resp_nva);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)resp_nva;
  fr.nvlen = nghttp3_arraylen(resp_nva);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  setup_default_client(&conn);
  nghttp3_conn_create_stream(conn, &stream, 0);
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, sconsumed);

  stream = nghttp3_conn_find_stream(conn, 0);

  assert_true(stream->flags & NGHTTP3_STREAM_FLAG_HTTP_ERROR);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* Receiving 2 trailers HEADERS is invalid*/
  nghttp3_buf_reset(&buf);
  nghttp3_qpack_encoder_init(&qenc, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)resp_nva;
  fr.nvlen = nghttp3_arraylen(resp_nva);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)trnv;
  fr.nvlen = nghttp3_arraylen(trnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);
  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  setup_default_client(&conn);
  nghttp3_conn_create_stream(conn, &stream, 0);
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff(NGHTTP3_ERR_H3_FRAME_UNEXPECTED, ==, sconsumed);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* We don't expect response trailers after HEADERS with CONNECT
     request */
  nghttp3_buf_reset(&buf);
  nghttp3_qpack_encoder_init(&qenc, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)resp_nva;
  fr.nvlen = nghttp3_arraylen(resp_nva);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)trnv;
  fr.nvlen = nghttp3_arraylen(trnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  setup_default_client(&conn);
  nghttp3_conn_create_stream(conn, &stream, 0);
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;
  nghttp3_http_record_request_method(stream, connect_reqnv,
                                     nghttp3_arraylen(connect_reqnv));

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff(NGHTTP3_ERR_H3_FRAME_UNEXPECTED, ==, sconsumed);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* We don't expect response trailers after DATA with CONNECT
     request */
  nghttp3_buf_reset(&buf);
  nghttp3_qpack_encoder_init(&qenc, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)resp_nva;
  fr.nvlen = nghttp3_arraylen(resp_nva);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);
  nghttp3_write_frame_data(&buf, 99);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)trnv;
  fr.nvlen = nghttp3_arraylen(trnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  setup_default_client(&conn);
  nghttp3_conn_create_stream(conn, &stream, 0);
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;
  nghttp3_http_record_request_method(stream, connect_reqnv,
                                     nghttp3_arraylen(connect_reqnv));

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff(NGHTTP3_ERR_H3_FRAME_UNEXPECTED, ==, sconsumed);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* request followed by trailers */
  nghttp3_buf_reset(&buf);
  nghttp3_qpack_encoder_init(&qenc, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)reqnv;
  fr.nvlen = nghttp3_arraylen(reqnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)trnv;
  fr.nvlen = nghttp3_arraylen(trnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  setup_default_server(&conn);
  nghttp3_conn_set_max_client_streams_bidi(conn, 1);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, sconsumed);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* request followed by trailers which contains pseudo headers */
  nghttp3_buf_reset(&buf);
  nghttp3_qpack_encoder_init(&qenc, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)reqnv;
  fr.nvlen = nghttp3_arraylen(reqnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);
  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  setup_default_server(&conn);
  nghttp3_conn_set_max_client_streams_bidi(conn, 1);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, sconsumed);

  stream = nghttp3_conn_find_stream(conn, 0);

  assert_true(stream->flags & NGHTTP3_STREAM_FLAG_HTTP_ERROR);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* request followed by 2 trailers */
  nghttp3_buf_reset(&buf);
  nghttp3_qpack_encoder_init(&qenc, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)reqnv;
  fr.nvlen = nghttp3_arraylen(reqnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)trnv;
  fr.nvlen = nghttp3_arraylen(trnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);
  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  setup_default_server(&conn);
  nghttp3_conn_set_max_client_streams_bidi(conn, 1);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff(NGHTTP3_ERR_H3_FRAME_UNEXPECTED, ==, sconsumed);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* We don't expect trailers after HEADERS with CONNECT request */
  nghttp3_buf_reset(&buf);
  nghttp3_qpack_encoder_init(&qenc, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)connect_reqnv;
  fr.nvlen = nghttp3_arraylen(connect_reqnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)trnv;
  fr.nvlen = nghttp3_arraylen(trnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  setup_default_server(&conn);
  nghttp3_conn_set_max_client_streams_bidi(conn, 1);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff(NGHTTP3_ERR_H3_FRAME_UNEXPECTED, ==, sconsumed);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* We don't expect trailers after DATA with CONNECT request */
  nghttp3_buf_reset(&buf);
  nghttp3_qpack_encoder_init(&qenc, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)connect_reqnv;
  fr.nvlen = nghttp3_arraylen(connect_reqnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);
  nghttp3_write_frame_data(&buf, 11);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)trnv;
  fr.nvlen = nghttp3_arraylen(trnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  setup_default_server(&conn);
  nghttp3_conn_set_max_client_streams_bidi(conn, 1);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff(NGHTTP3_ERR_H3_FRAME_UNEXPECTED, ==, sconsumed);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* server: content-length in request trailers is ignored and
     removed. */
  nghttp3_buf_reset(&buf);
  nghttp3_qpack_encoder_init(&qenc, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)reqnv;
  fr.nvlen = nghttp3_arraylen(reqnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)clnv;
  fr.nvlen = nghttp3_arraylen(clnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.recv_trailer = recv_trailer;

  conn_options_clear(&opts);
  opts.callbacks = &callbacks;
  opts.user_data = &ud;

  setup_default_server_with_options(&conn, opts);

  ud.recv_trailer_cb.ncalled = 0;
  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, sconsumed);
  assert_size(0, ==, ud.recv_trailer_cb.ncalled);

  stream = nghttp3_conn_find_stream(conn, 0);

  assert_false(stream->flags & NGHTTP3_STREAM_FLAG_HTTP_ERROR);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* client: content-length in request trailers is ignored and
     removed. */
  nghttp3_buf_reset(&buf);
  nghttp3_qpack_encoder_init(&qenc, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)resp_nva;
  fr.nvlen = nghttp3_arraylen(resp_nva);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)clnv;
  fr.nvlen = nghttp3_arraylen(clnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.recv_trailer = recv_trailer;

  conn_options_clear(&opts);
  opts.callbacks = &callbacks;
  opts.user_data = &ud;

  setup_default_client_with_options(&conn, opts);
  nghttp3_conn_create_stream(conn, &stream, 0);
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;

  ud.recv_trailer_cb.ncalled = 0;
  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, sconsumed);
  assert_size(0, ==, ud.recv_trailer_cb.ncalled);

  stream = nghttp3_conn_find_stream(conn, 0);

  assert_false(stream->flags & NGHTTP3_STREAM_FLAG_HTTP_ERROR);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);
}

void test_nghttp3_conn_http_ignore_content_length(void) {
  uint8_t rawbuf[4096];
  nghttp3_buf buf;
  nghttp3_frame_headers fr;
  nghttp3_conn *conn;
  const nghttp3_mem *mem = nghttp3_mem_default();
  nghttp3_ssize sconsumed;
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

  /* If status code is 304, content-length must be ignored. */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)resnv;
  fr.nvlen = nghttp3_arraylen(resnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  setup_default_client(&conn);
  nghttp3_conn_create_stream(conn, &stream, 0);
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, sconsumed);
  assert_int64(0, ==, stream->rx.http.content_length);

  rv = nghttp3_conn_close_stream(conn, 0, NGHTTP3_H3_NO_ERROR);

  assert_int(0, ==, rv);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* If method is CONNECT, content-length must be ignored. */
  nghttp3_buf_reset(&buf);
  nghttp3_qpack_encoder_init(&qenc, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)reqnv;
  fr.nvlen = nghttp3_arraylen(reqnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  setup_default_server(&conn);
  nghttp3_conn_set_max_client_streams_bidi(conn, 1);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, sconsumed);

  stream = nghttp3_conn_find_stream(conn, 0);

  assert_int64(-1, ==, stream->rx.http.content_length);

  rv = nghttp3_conn_close_stream(conn, 0, NGHTTP3_H3_NO_ERROR);

  assert_int(0, ==, rv);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* Content-Length in 200 response to CONNECT is ignored */
  nghttp3_buf_reset(&buf);
  nghttp3_qpack_encoder_init(&qenc, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)cl_resnv;
  fr.nvlen = nghttp3_arraylen(cl_resnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  setup_default_client(&conn);
  nghttp3_conn_create_stream(conn, &stream, 0);
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;
  stream->rx.http.flags |= NGHTTP3_HTTP_FLAG_METH_CONNECT;

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, sconsumed);
  assert_int64(-1, ==, stream->rx.http.content_length);

  rv = nghttp3_conn_close_stream(conn, 0, NGHTTP3_H3_NO_ERROR);

  assert_int(0, ==, rv);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);
}

void test_nghttp3_conn_http_record_request_method(void) {
  uint8_t rawbuf[4096];
  nghttp3_buf buf;
  nghttp3_frame_headers fr;
  nghttp3_conn *conn;
  const nghttp3_mem *mem = nghttp3_mem_default();
  nghttp3_ssize sconsumed;
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

  /* content-length is not allowed with 200 status code in response to
     CONNECT request.  Just ignore it. */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)resnv;
  fr.nvlen = nghttp3_arraylen(resnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  setup_default_client(&conn);
  nghttp3_conn_create_stream(conn, &stream, 0);
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;
  nghttp3_http_record_request_method(stream, connect_reqnv,
                                     nghttp3_arraylen(connect_reqnv));

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, sconsumed);
  assert_int64(-1, ==, stream->rx.http.content_length);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* The content-length in response to HEAD request must be
     ignored. */
  nghttp3_buf_reset(&buf);
  nghttp3_qpack_encoder_init(&qenc, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)resnv;
  fr.nvlen = nghttp3_arraylen(resnv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  setup_default_client(&conn);
  nghttp3_conn_create_stream(conn, &stream, 0);
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;
  nghttp3_http_record_request_method(stream, head_reqnv,
                                     nghttp3_arraylen(head_reqnv));

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, sconsumed);
  assert_int64(0, ==, stream->rx.http.content_length);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);
}

void test_nghttp3_conn_http_error(void) {
  uint8_t rawbuf[4096];
  nghttp3_buf buf, ebuf;
  nghttp3_frame_headers fr;
  nghttp3_conn *conn;
  nghttp3_callbacks callbacks = {
    .stop_sending = stop_sending,
    .reset_stream = reset_stream,
  };
  const nghttp3_mem *mem = nghttp3_mem_default();
  nghttp3_ssize sconsumed;
  nghttp3_qpack_encoder qenc;
  nghttp3_settings settings;
  const nghttp3_nv dupschemenv[] = {
    MAKE_NV(":path", "/"),
    MAKE_NV(":method", "GET"),
    MAKE_NV(":authority", "localhost"),
    MAKE_NV(":scheme", "https"),
    MAKE_NV(":scheme", "https"),
  };
  const nghttp3_nv noschemenv[] = {
    MAKE_NV(":path", "/"),
    MAKE_NV(":method", "GET"),
    MAKE_NV(":authority", "localhost"),
  };
  userdata ud = {0};
  nghttp3_stream *stream;
  conn_options opts;

  nghttp3_settings_default(&settings);
  settings.qpack_max_dtable_capacity = 4096;
  settings.qpack_blocked_streams = 100;

  /* duplicated :scheme */
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));
  nghttp3_qpack_encoder_init(&qenc, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)dupschemenv;
  fr.nvlen = nghttp3_arraylen(dupschemenv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  conn_options_clear(&opts);
  opts.callbacks = &callbacks;
  opts.settings = &settings;
  opts.user_data = &ud;

  setup_default_server_with_options(&conn, opts);
  nghttp3_conn_set_max_client_streams_bidi(conn, 1);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, sconsumed);
  assert_size(1, ==, ud.stop_sending_cb.ncalled);
  assert_int64(0, ==, ud.stop_sending_cb.stream_id);
  assert_uint64(NGHTTP3_H3_MESSAGE_ERROR, ==,
                ud.stop_sending_cb.app_error_code);
  assert_size(1, ==, ud.reset_stream_cb.ncalled);
  assert_int64(0, ==, ud.reset_stream_cb.stream_id);
  assert_uint64(NGHTTP3_H3_MESSAGE_ERROR, ==,
                ud.reset_stream_cb.app_error_code);

  stream = nghttp3_conn_find_stream(conn, 0);

  assert_true(stream->flags & NGHTTP3_STREAM_FLAG_HTTP_ERROR);

  /* After the error, everything is just discarded. */
  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, sconsumed);
  assert_size(1, ==, ud.stop_sending_cb.ncalled);
  assert_size(1, ==, ud.reset_stream_cb.ncalled);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* without :scheme */
  nghttp3_buf_reset(&buf);
  nghttp3_qpack_encoder_init(&qenc, 0, mem);
  memset(&ud, 0, sizeof(ud));

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)noschemenv;
  fr.nvlen = nghttp3_arraylen(noschemenv);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  conn_options_clear(&opts);
  opts.callbacks = &callbacks;
  opts.settings = &settings;
  opts.user_data = &ud;

  setup_default_server_with_options(&conn, opts);
  nghttp3_conn_set_max_client_streams_bidi(conn, 1);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, sconsumed);
  assert_size(1, ==, ud.stop_sending_cb.ncalled);
  assert_int64(0, ==, ud.stop_sending_cb.stream_id);
  assert_uint64(NGHTTP3_H3_MESSAGE_ERROR, ==,
                ud.stop_sending_cb.app_error_code);
  assert_size(1, ==, ud.reset_stream_cb.ncalled);
  assert_int64(0, ==, ud.reset_stream_cb.stream_id);
  assert_uint64(NGHTTP3_H3_MESSAGE_ERROR, ==,
                ud.reset_stream_cb.app_error_code);

  stream = nghttp3_conn_find_stream(conn, 0);

  assert_true(stream->flags & NGHTTP3_STREAM_FLAG_HTTP_ERROR);

  /* After the error, everything is just discarded. */
  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, sconsumed);
  assert_size(1, ==, ud.stop_sending_cb.ncalled);
  assert_size(1, ==, ud.reset_stream_cb.ncalled);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* error on blocked stream */
  nghttp3_buf_reset(&buf);
  nghttp3_qpack_encoder_init(&qenc, settings.qpack_max_dtable_capacity, mem);
  nghttp3_qpack_encoder_set_max_blocked_streams(&qenc,
                                                settings.qpack_blocked_streams);
  nghttp3_qpack_encoder_set_max_dtable_capacity(
    &qenc, settings.qpack_max_dtable_capacity);
  memset(&ud, 0, sizeof(ud));

  nghttp3_buf_init(&ebuf);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.nva = (nghttp3_nv *)noschemenv;
  fr.nvlen = nghttp3_arraylen(noschemenv);

  nghttp3_write_frame_qpack_dyn(&buf, &ebuf, &qenc, 0, (nghttp3_frame *)&fr);

  conn_options_clear(&opts);
  opts.callbacks = &callbacks;
  opts.settings = &settings;
  opts.user_data = &ud;

  setup_default_server_with_options(&conn, opts);
  nghttp3_conn_set_max_client_streams_bidi(conn, 1);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff(0, <, sconsumed);
  assert_ptrdiff(sconsumed, !=, (nghttp3_ssize)nghttp3_buf_len(&buf));
  assert_size(0, ==, ud.stop_sending_cb.ncalled);
  assert_size(0, ==, ud.reset_stream_cb.ncalled);

  stream = nghttp3_conn_find_stream(conn, 0);

  assert_false(stream->flags & NGHTTP3_STREAM_FLAG_HTTP_ERROR);
  assert_size(0, !=, nghttp3_ringbuf_len(&stream->inq));

  nghttp3_buf_reset(&buf);
  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_QPACK_ENCODER);

  sconsumed = nghttp3_conn_read_stream(conn, 6, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, sconsumed);

  sconsumed = nghttp3_conn_read_stream(conn, 6, ebuf.pos,
                                       nghttp3_buf_len(&ebuf), /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&ebuf), ==, sconsumed);
  assert_true(stream->flags & NGHTTP3_STREAM_FLAG_HTTP_ERROR);
  assert_size(0, ==, nghttp3_ringbuf_len(&stream->inq));
  assert_size(1, ==, ud.stop_sending_cb.ncalled);
  assert_int64(0, ==, ud.stop_sending_cb.stream_id);
  assert_uint64(NGHTTP3_H3_MESSAGE_ERROR, ==,
                ud.stop_sending_cb.app_error_code);
  assert_size(1, ==, ud.reset_stream_cb.ncalled);
  assert_int64(0, ==, ud.reset_stream_cb.stream_id);
  assert_uint64(NGHTTP3_H3_MESSAGE_ERROR, ==,
                ud.reset_stream_cb.app_error_code);

  /* After the error, everything is just discarded. */
  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, sconsumed);
  assert_size(1, ==, ud.stop_sending_cb.ncalled);
  assert_size(1, ==, ud.reset_stream_cb.ncalled);

  nghttp3_buf_free(&ebuf, mem);
  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);
}

void test_nghttp3_conn_qpack_blocked_stream(void) {
  const nghttp3_mem *mem = nghttp3_mem_default();
  nghttp3_conn *conn;
  nghttp3_settings settings;
  nghttp3_qpack_encoder qenc;
  int rv;
  nghttp3_buf ebuf;
  uint8_t rawbuf[4096];
  nghttp3_buf buf;
  nghttp3_frame fr;
  nghttp3_ssize sconsumed;
  size_t buffered_datalen;
  nghttp3_stream *stream;
  conn_options opts;

  nghttp3_settings_default(&settings);
  settings.qpack_max_dtable_capacity = 4096;
  settings.qpack_blocked_streams = 100;

  /* The deletion of QPACK blocked stream is deferred to the moment
     when it is unblocked */
  nghttp3_buf_init(&ebuf);
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));

  nghttp3_qpack_encoder_init(&qenc, settings.qpack_max_dtable_capacity, mem);
  nghttp3_qpack_encoder_set_max_blocked_streams(&qenc,
                                                settings.qpack_blocked_streams);
  nghttp3_qpack_encoder_set_max_dtable_capacity(
    &qenc, settings.qpack_max_dtable_capacity);

  conn_options_clear(&opts);
  opts.settings = &settings;

  setup_default_client_with_options(&conn, opts);
  nghttp3_conn_bind_qpack_streams(conn, 2, 6);

  rv = nghttp3_conn_submit_request(conn, 0, req_nva, nghttp3_arraylen(req_nva),
                                   NULL, NULL);

  assert_int(0, ==, rv);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.headers.nva = (nghttp3_nv *)resp_nva;
  fr.headers.nvlen = nghttp3_arraylen(resp_nva);

  nghttp3_write_frame_qpack_dyn(&buf, &ebuf, &qenc, 0, &fr);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff(0, <, sconsumed);
  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), !=, sconsumed);

  buffered_datalen = nghttp3_buf_len(&buf) - (size_t)sconsumed;

  nghttp3_buf_reset(&buf);
  nghttp3_write_frame_data(&buf, 1111);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 1);

  assert_ptrdiff(0, ==, sconsumed);

  buffered_datalen += nghttp3_buf_len(&buf);
  stream = nghttp3_conn_find_stream(conn, 0);

  assert_size(buffered_datalen, ==,
              nghttp3_stream_get_buffered_datalen(stream));

  rv = nghttp3_conn_close_stream(conn, 0, NGHTTP3_H3_NO_ERROR);

  assert_int(0, ==, rv);

  assert_true(stream->flags & NGHTTP3_STREAM_FLAG_CLOSED);

  nghttp3_buf_reset(&buf);
  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_QPACK_ENCODER);

  sconsumed = nghttp3_conn_read_stream(conn, 7, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, sconsumed);
  assert_not_null(nghttp3_conn_find_stream(conn, 0));

  sconsumed = nghttp3_conn_read_stream(conn, 7, ebuf.pos,
                                       nghttp3_buf_len(&ebuf), /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&ebuf), ==, sconsumed);
  assert_null(nghttp3_conn_find_stream(conn, 0));

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);
  nghttp3_buf_free(&ebuf, mem);

  /* Stream that is blocked receives HEADERS which has empty
     representation (that is only include Header Block Prefix) */
  nghttp3_buf_init(&ebuf);
  nghttp3_buf_reset(&buf);

  nghttp3_qpack_encoder_init(&qenc, settings.qpack_max_dtable_capacity, mem);
  nghttp3_qpack_encoder_set_max_blocked_streams(&qenc,
                                                settings.qpack_blocked_streams);
  nghttp3_qpack_encoder_set_max_dtable_capacity(
    &qenc, settings.qpack_max_dtable_capacity);

  conn_options_clear(&opts);
  opts.settings = &settings;

  setup_default_client_with_options(&conn, opts);
  nghttp3_conn_bind_qpack_streams(conn, 2, 6);

  rv = nghttp3_conn_submit_request(conn, 0, req_nva, nghttp3_arraylen(req_nva),
                                   NULL, NULL);

  assert_int(0, ==, rv);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.headers.nva = (nghttp3_nv *)resp_nva;
  fr.headers.nvlen = nghttp3_arraylen(resp_nva);

  nghttp3_write_frame_qpack_dyn(&buf, &ebuf, &qenc, 0, &fr);

  assert(nghttp3_buf_len(&buf) > 4);

  /* Craft empty HEADERS (just leave Header Block Prefix) */
  buf.pos[1] = 2;
  /* Write garbage to continue to read stream */
  buf.pos[4] = 0xff;

  sconsumed = nghttp3_conn_read_stream(
    conn, 0, buf.pos, 5 /* Frame header + Header Block Prefix */,
    /* fin = */ 1);

  assert_ptrdiff(0, <, sconsumed);
  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), !=, sconsumed);

  rv = nghttp3_conn_close_stream(conn, 0, NGHTTP3_H3_NO_ERROR);

  assert_int(0, ==, rv);

  stream = nghttp3_conn_find_stream(conn, 0);

  assert_true(stream->flags & NGHTTP3_STREAM_FLAG_CLOSED);

  nghttp3_buf_reset(&buf);
  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_QPACK_ENCODER);

  sconsumed = nghttp3_conn_read_stream(conn, 7, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, sconsumed);
  assert_not_null(nghttp3_conn_find_stream(conn, 0));

  sconsumed = nghttp3_conn_read_stream(conn, 7, ebuf.pos,
                                       nghttp3_buf_len(&ebuf), /* fin = */ 0);

  assert_ptrdiff(NGHTTP3_ERR_QPACK_DECOMPRESSION_FAILED, ==, sconsumed);
  assert_not_null(nghttp3_conn_find_stream(conn, 0));

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);
  nghttp3_buf_free(&ebuf, mem);
}

void test_nghttp3_conn_qpack_decoder_cancel_stream(void) {
  nghttp3_conn *conn;
  nghttp3_vec vec[256];
  nghttp3_ssize sveccnt;
  nghttp3_buf *buf;
  int64_t stream_id;
  int fin;
  size_t i;
  int rv;

  /* Cancel streams and make QPACK decoder buffer grow, which makes
     conn use custom sized shared buffer. */
  setup_default_client(&conn);
  conn_write_initial_streams(conn);

  for (i = 0; i < NGHTTP3_STREAM_MIN_CHUNK_SIZE; ++i) {
    rv = nghttp3_qpack_decoder_cancel_stream(&conn->qdec, (int64_t)i);

    assert_int(0, ==, rv);
  }

  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  assert_int64(conn->tx.qdec->node.id, ==, stream_id);
  assert_ptrdiff(1, ==, sveccnt);

  buf = nghttp3_ringbuf_get(&conn->tx.qdec->chunks,
                            nghttp3_ringbuf_len(&conn->tx.qdec->chunks) - 1);

  assert_size(NGHTTP3_STREAM_MIN_CHUNK_SIZE, <, nghttp3_buf_cap(buf));

  rv = nghttp3_conn_add_write_offset(
    conn, stream_id, (size_t)nghttp3_vec_len(vec, (size_t)sveccnt));

  assert_int(0, ==, rv);

  rv = nghttp3_conn_add_ack_offset(conn, stream_id,
                                   nghttp3_vec_len(vec, (size_t)sveccnt));

  assert_int(0, ==, rv);

  nghttp3_conn_del(conn);
}

void test_nghttp3_conn_just_fin(void) {
  nghttp3_conn *conn;
  nghttp3_vec vec[256];
  nghttp3_ssize sveccnt;
  int rv;
  int64_t stream_id;
  nghttp3_data_reader dr;
  int fin;
  userdata ud = {0};
  conn_options opts;

  conn_options_clear(&opts);
  opts.user_data = &ud;

  setup_default_client_with_options(&conn, opts);
  conn_write_initial_streams(conn);

  /* No DATA frame header */
  dr.read_data = step_read_data;
  rv = nghttp3_conn_submit_request(conn, 0, req_nva, nghttp3_arraylen(req_nva),
                                   &dr, NULL);

  assert_int(0, ==, rv);

  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  assert_ptrdiff(1, ==, sveccnt);
  assert_int64(0, ==, stream_id);
  assert_true(fin);

  rv = nghttp3_conn_add_write_offset(
    conn, stream_id, (size_t)nghttp3_vec_len(vec, (size_t)sveccnt));

  assert_int(0, ==, rv);

  /* Just fin */
  ud.data.nblock = 1;
  dr.read_data = block_then_step_read_data;

  rv = nghttp3_conn_submit_request(conn, 4, req_nva, nghttp3_arraylen(req_nva),
                                   &dr, NULL);

  assert_int(0, ==, rv);

  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  assert_ptrdiff(1, ==, sveccnt);
  assert_int64(4, ==, stream_id);
  assert_false(fin);

  rv = nghttp3_conn_add_write_offset(
    conn, stream_id, (size_t)nghttp3_vec_len(vec, (size_t)sveccnt));

  assert_int(0, ==, rv);

  /* Resume stream 4 because it was blocked */
  nghttp3_conn_resume_stream(conn, 4);

  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  assert_ptrdiff(0, ==, sveccnt);
  assert_int64(4, ==, stream_id);
  assert_true(fin);

  rv = nghttp3_conn_add_write_offset(
    conn, stream_id, (size_t)nghttp3_vec_len(vec, (size_t)sveccnt));

  assert_int(0, ==, rv);

  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  assert_ptrdiff(0, ==, sveccnt);
  assert_int64(-1, ==, stream_id);
  assert_false(fin);

  nghttp3_conn_del(conn);
}

void test_nghttp3_conn_submit_response_read_blocked(void) {
  nghttp3_conn *conn;
  nghttp3_stream *stream;
  int rv;
  nghttp3_vec vec[256];
  int fin;
  int64_t stream_id;
  nghttp3_ssize sveccnt;
  nghttp3_data_reader dr = {step_then_block_read_data};
  userdata ud;
  conn_options opts;

  /* Make sure that flushing serialized data while
     NGHTTP3_STREAM_FLAG_READ_DATA_BLOCKED is set does not cause any
     error */
  conn_options_clear(&opts);
  opts.user_data = &ud;

  setup_default_server_with_options(&conn, opts);
  conn_write_initial_streams(conn);
  conn->remote.bidi.max_client_streams = 1;

  nghttp3_conn_create_stream(conn, &stream, 0);

  ud.data.left = 1000;
  ud.data.step = 1000;
  rv = nghttp3_conn_submit_response(conn, 0, resp_nva,
                                    nghttp3_arraylen(resp_nva), &dr);

  assert_int(0, ==, rv);

  for (;;) {
    sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                         nghttp3_arraylen(vec));

    assert_ptrdiff(0, <=, sveccnt);

    if (sveccnt <= 0) {
      break;
    }

    rv = nghttp3_conn_add_write_offset(conn, stream_id, 1);

    assert_int(0, ==, rv);
  }

  nghttp3_conn_del(conn);
}

void test_nghttp3_conn_submit_info(void) {
  nghttp3_conn *conn;
  const nghttp3_nv nva[] = {
    MAKE_NV("foo", "bar"),
  };
  nghttp3_stream *stream;
  int rv;
  nghttp3_vec vec[256];
  int fin;
  int64_t stream_id;
  nghttp3_ssize sveccnt;

  setup_default_server(&conn);
  conn_write_initial_streams(conn);

  nghttp3_conn_create_stream(conn, &stream, 0);

  rv = nghttp3_conn_submit_info(conn, 0, nva, nghttp3_arraylen(nva));

  assert_int(0, ==, rv);

  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  assert_ptrdiff(0, <, sveccnt);

  nghttp3_conn_del(conn);

  /* Submitting non-final response against non-existing stream is
     treated as error. */
  setup_default_server(&conn);

  rv = nghttp3_conn_submit_info(conn, 0, nva, nghttp3_arraylen(nva));

  assert_int(NGHTTP3_ERR_STREAM_NOT_FOUND, ==, rv);

  nghttp3_conn_del(conn);
}

void test_nghttp3_conn_recv_uni(void) {
  nghttp3_conn *conn;
  nghttp3_ssize nread;
  uint8_t buf[256];

  /* 0 length unidirectional stream must be ignored */
  setup_default_client(&conn);

  nread = nghttp3_conn_read_stream(conn, 3, NULL, 0, /* fin = */ 1);

  assert_ptrdiff(0, ==, nread);
  assert_null(nghttp3_conn_find_stream(conn, 3));

  nghttp3_conn_del(conn);

  /* 0 length unidirectional stream; first get 0 length without fin,
     and then get 0 length with fin. */
  setup_default_client(&conn);

  nread = nghttp3_conn_read_stream(conn, 3, NULL, 0, /* fin = */ 0);

  assert_ptrdiff(0, ==, nread);
  assert_not_null(nghttp3_conn_find_stream(conn, 3));

  nread = nghttp3_conn_read_stream(conn, 3, NULL, 0, /* fin = */ 1);

  assert_ptrdiff(0, ==, nread);
  assert_null(nghttp3_conn_find_stream(conn, 3));

  nghttp3_conn_del(conn);

  /* Fin while reading stream header is treated as error. */
  setup_default_client(&conn);

  /* 4 bytes integer */
  buf[0] = 0xc0;
  nread = nghttp3_conn_read_stream(conn, 3, buf, 1, /* fin = */ 0);

  assert_ptrdiff(1, ==, nread);
  assert_not_null(nghttp3_conn_find_stream(conn, 3));

  nread = nghttp3_conn_read_stream(conn, 3, NULL, 0, /* fin = */ 1);

  assert_ptrdiff(NGHTTP3_ERR_H3_GENERAL_PROTOCOL_ERROR, ==, nread);

  nghttp3_conn_del(conn);

  /* Receiving a push stream is treated as error. */
  setup_default_client(&conn);

  buf[0] = NGHTTP3_STREAM_TYPE_PUSH;
  nread = nghttp3_conn_read_stream(conn, 3, buf, 1, /* fin = */ 0);

  assert_ptrdiff(NGHTTP3_ERR_H3_STREAM_CREATION_ERROR, ==, nread);

  nghttp3_conn_del(conn);
}

void test_nghttp3_conn_recv_goaway(void) {
  nghttp3_conn *conn;
  nghttp3_callbacks callbacks = {
    .shutdown = conn_shutdown,
  };
  nghttp3_frame fr;
  uint8_t rawbuf[1024];
  nghttp3_buf buf;
  nghttp3_ssize nconsumed;
  int rv;
  size_t i;
  nghttp3_stream *stream;
  userdata ud = {0};
  conn_options opts;

  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));

  /* Client receives GOAWAY */
  conn_options_clear(&opts);
  opts.callbacks = &callbacks;
  opts.user_data = &ud;

  setup_default_client_with_options(&conn, opts);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  fr.hd.type = NGHTTP3_FRAME_SETTINGS;
  fr.settings.niv = 0;

  nghttp3_write_frame(&buf, &fr);

  fr.hd.type = NGHTTP3_FRAME_GOAWAY;
  fr.goaway.id = 12;

  nghttp3_write_frame(&buf, &fr);

  ud.shutdown_cb.ncalled = 0;
  ud.shutdown_cb.id = 0;
  nconsumed = nghttp3_conn_read_stream(conn, 3, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, nconsumed);
  assert_true(conn->flags & NGHTTP3_CONN_FLAG_GOAWAY_RECVED);
  assert_int64(12, ==, conn->rx.goaway_id);
  assert_size(1, ==, ud.shutdown_cb.ncalled);
  assert_int64(12, ==, ud.shutdown_cb.id);

  /* Cannot submit request anymore */
  rv = nghttp3_conn_submit_request(conn, 0, req_nva, nghttp3_arraylen(req_nva),
                                   NULL, NULL);

  assert_int(NGHTTP3_ERR_CONN_CLOSING, ==, rv);

  nghttp3_conn_del(conn);

  /* Receiving GOAWAY with increased ID is treated as error */
  nghttp3_buf_reset(&buf);
  conn_options_clear(&opts);
  opts.callbacks = &callbacks;
  opts.user_data = &ud;

  setup_default_client_with_options(&conn, opts);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  fr.hd.type = NGHTTP3_FRAME_SETTINGS;
  fr.settings.niv = 0;

  nghttp3_write_frame(&buf, &fr);

  fr.hd.type = NGHTTP3_FRAME_GOAWAY;
  fr.goaway.id = 12;

  nghttp3_write_frame(&buf, &fr);

  fr.hd.type = NGHTTP3_FRAME_GOAWAY;
  fr.goaway.id = 16;

  nghttp3_write_frame(&buf, &fr);

  ud.shutdown_cb.ncalled = 0;
  ud.shutdown_cb.id = 0;
  nconsumed = nghttp3_conn_read_stream(conn, 3, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff(NGHTTP3_ERR_H3_ID_ERROR, ==, nconsumed);
  assert_true(conn->flags & NGHTTP3_CONN_FLAG_GOAWAY_RECVED);
  assert_int64(12, ==, conn->rx.goaway_id);
  assert_size(1, ==, ud.shutdown_cb.ncalled);
  assert_int64(12, ==, ud.shutdown_cb.id);

  nghttp3_conn_del(conn);

  /* Server receives GOAWAY */
  nghttp3_buf_reset(&buf);
  conn_options_clear(&opts);
  opts.callbacks = &callbacks;
  opts.user_data = &ud;

  setup_default_server_with_options(&conn, opts);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  fr.hd.type = NGHTTP3_FRAME_SETTINGS;
  fr.settings.niv = 0;

  nghttp3_write_frame(&buf, &fr);

  fr.hd.type = NGHTTP3_FRAME_GOAWAY;
  fr.goaway.id = 101;

  nghttp3_write_frame(&buf, &fr);

  ud.shutdown_cb.ncalled = 0;
  ud.shutdown_cb.id = 0;
  nconsumed = nghttp3_conn_read_stream(conn, 2, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, nconsumed);
  assert_true(conn->flags & NGHTTP3_CONN_FLAG_GOAWAY_RECVED);
  assert_int64(101, ==, conn->rx.goaway_id);
  assert_size(1, ==, ud.shutdown_cb.ncalled);
  assert_int64(101, ==, ud.shutdown_cb.id);

  nghttp3_conn_del(conn);

  /* Receive GOAWAY without Stream ID */
  nghttp3_buf_reset(&buf);
  setup_default_server(&conn);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  fr.hd.type = NGHTTP3_FRAME_SETTINGS;
  fr.settings.niv = 0;

  nghttp3_write_frame(&buf, (nghttp3_frame *)&fr);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_FRAME_GOAWAY);
  buf.last = nghttp3_put_varint(buf.last, 0);

  nconsumed = nghttp3_conn_read_stream(conn, 2, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff(NGHTTP3_ERR_H3_FRAME_ERROR, ==, nconsumed);

  nghttp3_conn_del(conn);

  /* Client receives GOAWAY with server bidirectional stream ID */
  nghttp3_buf_reset(&buf);
  setup_default_client(&conn);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  fr.hd.type = NGHTTP3_FRAME_SETTINGS;
  fr.settings.niv = 0;

  nghttp3_write_frame(&buf, (nghttp3_frame *)&fr);

  fr.hd.type = NGHTTP3_FRAME_GOAWAY;
  fr.goaway.id = 1;

  nghttp3_write_frame(&buf, (nghttp3_frame *)&fr);

  nconsumed = nghttp3_conn_read_stream(conn, 3, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff(NGHTTP3_ERR_H3_ID_ERROR, ==, nconsumed);

  nghttp3_conn_del(conn);

  /* Read GOAWAY in 1 byte at a time */
  nghttp3_buf_reset(&buf);
  setup_default_server(&conn);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  fr.hd.type = NGHTTP3_FRAME_SETTINGS;
  fr.settings.niv = 0;

  nghttp3_write_frame(&buf, (nghttp3_frame *)&fr);

  fr.hd.type = NGHTTP3_FRAME_GOAWAY;
  fr.goaway.id = 0xff1;

  nghttp3_write_frame(&buf, (nghttp3_frame *)&fr);

  for (i = 0; i < nghttp3_buf_len(&buf); ++i) {
    nconsumed = nghttp3_conn_read_stream(conn, 2, buf.pos + i, 1,
                                         /* fin = */ 0);

    assert_ptrdiff(1, ==, nconsumed);
  }

  stream = nghttp3_conn_find_stream(conn, 2);

  assert_int(NGHTTP3_CTRL_STREAM_STATE_FRAME_TYPE, ==, stream->rstate.state);

  nghttp3_conn_del(conn);
}

void test_nghttp3_conn_shutdown_server(void) {
  const nghttp3_mem *mem = nghttp3_mem_default();
  nghttp3_conn *conn;
  nghttp3_callbacks callbacks = {
    .stop_sending = stop_sending,
    .reset_stream = reset_stream,
  };
  nghttp3_frame fr;
  uint8_t rawbuf[1024];
  nghttp3_buf buf;
  nghttp3_ssize nconsumed;
  nghttp3_stream *stream;
  nghttp3_qpack_encoder qenc;
  int rv;
  userdata ud;
  nghttp3_ssize sveccnt;
  nghttp3_vec vec[256];
  int64_t stream_id;
  int fin;
  conn_options opts;

  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));

  /* Server sends GOAWAY and rejects stream whose ID is greater than
     or equal to the ID in GOAWAY. */
  conn_options_clear(&opts);
  opts.callbacks = &callbacks;
  opts.user_data = &ud;

  setup_default_server_with_options(&conn, opts);
  conn_write_initial_streams(conn);
  nghttp3_qpack_encoder_init(&qenc, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.headers.nva = (nghttp3_nv *)req_nva;
  fr.headers.nvlen = nghttp3_arraylen(req_nva);

  nghttp3_write_frame_qpack(&buf, &qenc, 4, &fr);

  nconsumed = nghttp3_conn_read_stream(conn, 4, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, nconsumed);
  assert_int64(4, ==, conn->rx.max_stream_id_bidi);

  rv = nghttp3_conn_shutdown(conn);

  assert_int(0, ==, rv);
  assert_true(conn->flags & NGHTTP3_CONN_FLAG_GOAWAY_QUEUED);
  assert_int64(8, ==, conn->tx.goaway_id);

  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  assert_ptrdiff(0, <, sveccnt);
  assert_int64(3, ==, stream_id);

  nghttp3_buf_reset(&buf);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.headers.nva = (nghttp3_nv *)req_nva;
  fr.headers.nvlen = nghttp3_arraylen(req_nva);

  nghttp3_write_frame_qpack(&buf, &qenc, 8, &fr);

  memset(&ud, 0, sizeof(ud));
  nconsumed = nghttp3_conn_read_stream(conn, 8, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, nconsumed);
  assert_int64(8, ==, conn->rx.max_stream_id_bidi);
  assert_size(1, ==, ud.stop_sending_cb.ncalled);
  assert_int64(8, ==, ud.stop_sending_cb.stream_id);
  assert_uint64(NGHTTP3_H3_REQUEST_REJECTED, ==,
                ud.stop_sending_cb.app_error_code);
  assert_size(1, ==, ud.reset_stream_cb.ncalled);
  assert_int64(8, ==, ud.reset_stream_cb.stream_id);
  assert_uint64(NGHTTP3_H3_REQUEST_REJECTED, ==,
                ud.reset_stream_cb.app_error_code);

  stream = nghttp3_conn_find_stream(conn, 8);

  assert_int(NGHTTP3_REQ_STREAM_STATE_IGN_REST, ==, stream->rstate.state);

  nghttp3_qpack_encoder_free(&qenc);
  nghttp3_conn_del(conn);
}

void test_nghttp3_conn_shutdown_client(void) {
  nghttp3_conn *conn;
  nghttp3_callbacks callbacks = {
    .stop_sending = stop_sending,
    .reset_stream = reset_stream,
  };
  uint8_t rawbuf[1024];
  nghttp3_buf buf;
  int rv;
  userdata ud;
  nghttp3_ssize sveccnt;
  nghttp3_vec vec[256];
  int64_t stream_id;
  int fin;
  conn_options opts;

  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));

  /* Client sends GOAWAY and rejects PUSH_PROMISE whose ID is greater
     than or equal to the ID in GOAWAY. */
  conn_options_clear(&opts);
  opts.callbacks = &callbacks;
  opts.user_data = &ud;

  setup_default_client_with_options(&conn, opts);
  conn_write_initial_streams(conn);

  rv = nghttp3_conn_submit_request(conn, 0, req_nva, nghttp3_arraylen(req_nva),
                                   NULL, NULL);

  assert_int(0, ==, rv);

  rv = nghttp3_conn_shutdown(conn);

  assert_int(0, ==, rv);
  assert_int64(0, ==, conn->tx.goaway_id);

  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  assert_ptrdiff(0, <, sveccnt);
  assert_int64(2, ==, stream_id);

  nghttp3_conn_del(conn);
}

void test_nghttp3_conn_priority_update(void) {
  const nghttp3_mem *mem = nghttp3_mem_default();
  nghttp3_conn *conn;
  nghttp3_frame fr;
  nghttp3_ssize nconsumed;
  uint8_t rawbuf[2048];
  nghttp3_buf buf;
  nghttp3_stream *stream;
  int rv;
  nghttp3_qpack_encoder qenc;
  const nghttp3_nv nva[] = {
    MAKE_NV(":path", "/"),         MAKE_NV(":authority", "example.com"),
    MAKE_NV(":scheme", "https"),   MAKE_NV(":method", "GET"),
    MAKE_NV("priority", "u=5, i"),
  };
  size_t i;

  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));

  /* Receive PRIORITY_UPDATE and stream has not been created yet */
  setup_default_server(&conn);
  nghttp3_conn_set_max_client_streams_bidi(conn, 1);
  nghttp3_qpack_encoder_init(&qenc, 0, mem);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  fr.hd.type = NGHTTP3_FRAME_SETTINGS;
  fr.settings.niv = 0;

  nghttp3_write_frame(&buf, (nghttp3_frame *)&fr);

  fr.hd.type = NGHTTP3_FRAME_PRIORITY_UPDATE;
  fr.priority_update.pri_elem_id = 0;
  fr.priority_update.data = (uint8_t *)"u=2,i";
  fr.priority_update.datalen = strlen("u=2,i");

  nghttp3_write_frame(&buf, (nghttp3_frame *)&fr);

  nconsumed = nghttp3_conn_read_stream(conn, 2, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, nconsumed);

  stream = nghttp3_conn_find_stream(conn, 0);

  assert_not_null(stream);
  assert_true(stream->flags & NGHTTP3_STREAM_FLAG_PRIORITY_UPDATE_RECVED);
  assert_uint32(2, ==, stream->node.pri.urgency);
  assert_uint8(1, ==, stream->node.pri.inc);

  nghttp3_buf_reset(&buf);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.headers.nva = (nghttp3_nv *)nva;
  fr.headers.nvlen = nghttp3_arraylen(nva);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, &fr);

  nconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 1);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, nconsumed);

  /* priority header field should not override the value set by
     PRIORITY_UPDATE frame. */
  assert_uint32(2, ==, stream->node.pri.urgency);
  assert_uint8(1, ==, stream->node.pri.inc);

  nghttp3_qpack_encoder_free(&qenc);
  nghttp3_conn_del(conn);

  /* Receive PRIORITY_UPDATE with 0 length priority field and stream
     has not been created yet */
  nghttp3_buf_reset(&buf);
  setup_default_server(&conn);
  nghttp3_conn_set_max_client_streams_bidi(conn, 1);
  nghttp3_qpack_encoder_init(&qenc, 0, mem);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  fr.hd.type = NGHTTP3_FRAME_SETTINGS;
  fr.settings.niv = 0;

  nghttp3_write_frame(&buf, (nghttp3_frame *)&fr);

  fr.hd.type = NGHTTP3_FRAME_PRIORITY_UPDATE;
  fr.priority_update.pri_elem_id = 0;
  fr.priority_update.datalen = 0;

  nghttp3_write_frame(&buf, (nghttp3_frame *)&fr);

  nconsumed = nghttp3_conn_read_stream(conn, 2, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, nconsumed);

  stream = nghttp3_conn_find_stream(conn, 0);

  assert_not_null(stream);
  assert_true(stream->flags & NGHTTP3_STREAM_FLAG_PRIORITY_UPDATE_RECVED);
  assert_uint32(NGHTTP3_DEFAULT_URGENCY, ==, stream->node.pri.urgency);
  assert_uint8(0, ==, stream->node.pri.inc);

  nghttp3_buf_reset(&buf);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.headers.nva = (nghttp3_nv *)nva;
  fr.headers.nvlen = nghttp3_arraylen(nva);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, &fr);

  nconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 1);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, nconsumed);

  /* priority header field should not override the value set by
     PRIORITY_UPDATE frame. */
  assert_uint32(NGHTTP3_DEFAULT_URGENCY, ==, stream->node.pri.urgency);
  assert_uint8(0, ==, stream->node.pri.inc);

  nghttp3_qpack_encoder_free(&qenc);
  nghttp3_conn_del(conn);

  /* Receive PRIORITY_UPDATE and stream has been created */
  nghttp3_buf_reset(&buf);
  setup_default_server(&conn);
  nghttp3_conn_set_max_client_streams_bidi(conn, 1);

  rv = nghttp3_conn_create_stream(conn, &stream, 0);

  assert_int(0, ==, rv);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  fr.hd.type = NGHTTP3_FRAME_SETTINGS;
  fr.settings.niv = 0;

  nghttp3_write_frame(&buf, (nghttp3_frame *)&fr);

  fr.hd.type = NGHTTP3_FRAME_PRIORITY_UPDATE;
  fr.priority_update.pri_elem_id = 0;
  fr.priority_update.data = (uint8_t *)"u=6";
  fr.priority_update.datalen = strlen("u=6");

  nghttp3_write_frame(&buf, (nghttp3_frame *)&fr);

  nconsumed = nghttp3_conn_read_stream(conn, 2, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, nconsumed);
  assert_true(stream->flags & NGHTTP3_STREAM_FLAG_PRIORITY_UPDATE_RECVED);
  assert_uint32(6, ==, stream->node.pri.urgency);
  assert_uint8(0, ==, stream->node.pri.inc);

  nghttp3_conn_del(conn);

  /* Receive PRIORITY_UPDATE against non-existent push_promise */
  nghttp3_buf_reset(&buf);
  setup_default_server(&conn);
  nghttp3_conn_set_max_client_streams_bidi(conn, 1);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  fr.hd.type = NGHTTP3_FRAME_SETTINGS;
  fr.settings.niv = 0;

  nghttp3_write_frame(&buf, (nghttp3_frame *)&fr);

  fr.hd.type = NGHTTP3_FRAME_PRIORITY_UPDATE_PUSH_ID;
  fr.priority_update.pri_elem_id = 0;
  fr.priority_update.data = (uint8_t *)"u=6";
  fr.priority_update.datalen = strlen("u=6");

  nghttp3_write_frame(&buf, (nghttp3_frame *)&fr);

  nconsumed = nghttp3_conn_read_stream(conn, 2, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff(NGHTTP3_ERR_H3_ID_ERROR, ==, nconsumed);

  nghttp3_conn_del(conn);

  /* Receive PRIORITY_UPDATE and its Priority Field Value is larger
     than buffer */
  nghttp3_buf_reset(&buf);
  setup_default_server(&conn);
  nghttp3_conn_set_max_client_streams_bidi(conn, 1);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  fr.hd.type = NGHTTP3_FRAME_SETTINGS;
  fr.settings.niv = 0;

  nghttp3_write_frame(&buf, (nghttp3_frame *)&fr);

  fr.hd.type = NGHTTP3_FRAME_PRIORITY_UPDATE;
  fr.priority_update.pri_elem_id = 0;
  fr.priority_update.data = (uint8_t *)"u=2,i";
  fr.priority_update.datalen = strlen("u=2,i");

  nghttp3_frame_write_priority_update_len(&fr.hd.length, &fr.priority_update);
  fr.hd.length += 10;
  buf.last = nghttp3_frame_write_priority_update(buf.last, &fr.priority_update);
  memset(buf.last, ' ', 10);
  buf.last += 10;

  /* Make sure boundary check works when frame is fragmented. */
  nconsumed =
    nghttp3_conn_read_stream(conn, 2, buf.pos, nghttp3_buf_len(&buf) - 10,
                             /* fin = */ 0);
  stream = nghttp3_conn_find_stream(conn, 2);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf) - 10, ==, nconsumed);
  assert_int(NGHTTP3_CTRL_STREAM_STATE_PRIORITY_UPDATE, ==,
             stream->rstate.state);

  nconsumed =
    nghttp3_conn_read_stream(conn, 2, buf.pos + nconsumed, 10, /* fin = */ 0);

  assert_ptrdiff(10, ==, nconsumed);
  assert_int(NGHTTP3_CTRL_STREAM_STATE_FRAME_TYPE, ==, stream->rstate.state);
  assert_null(nghttp3_conn_find_stream(conn, 0));

  nghttp3_conn_del(conn);

  /* Ignore PRIORITY_UPDATE frame that has priority field longer than
     buffer size.  */
  nghttp3_buf_reset(&buf);
  setup_default_server(&conn);
  nghttp3_conn_set_max_client_streams_bidi(conn, 1);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  fr.hd.type = NGHTTP3_FRAME_SETTINGS;
  fr.settings.niv = 0;

  nghttp3_write_frame(&buf, (nghttp3_frame *)&fr);

  fr.hd.type = NGHTTP3_FRAME_PRIORITY_UPDATE;
  fr.priority_update.pri_elem_id = 0;
  fr.priority_update.data = (uint8_t *)"u=1,aaaaa";
  fr.priority_update.datalen = strlen("u=1,aaaaa");

  assert_size(sizeof(conn->rx.pri_fieldbuf), <, fr.priority_update.datalen);

  nghttp3_write_frame(&buf, (nghttp3_frame *)&fr);

  nconsumed = nghttp3_conn_read_stream(conn, 2, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, nconsumed);

  stream = nghttp3_conn_find_stream(conn, 0);

  assert_null(stream);

  nghttp3_conn_del(conn);

  /* Process PRIORITY_UPDATE frame that has priority field equal to
     buffer size.  */
  nghttp3_buf_reset(&buf);
  setup_default_server(&conn);
  nghttp3_conn_set_max_client_streams_bidi(conn, 1);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  fr.hd.type = NGHTTP3_FRAME_SETTINGS;
  fr.settings.niv = 0;

  nghttp3_write_frame(&buf, (nghttp3_frame *)&fr);

  fr.hd.type = NGHTTP3_FRAME_PRIORITY_UPDATE;
  fr.priority_update.pri_elem_id = 0;
  fr.priority_update.data = (uint8_t *)"u=1,aaaa";
  fr.priority_update.datalen = strlen("u=1,aaaa");

  assert_size(sizeof(conn->rx.pri_fieldbuf), ==, fr.priority_update.datalen);

  nghttp3_write_frame(&buf, (nghttp3_frame *)&fr);

  nconsumed = nghttp3_conn_read_stream(conn, 2, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, nconsumed);

  stream = nghttp3_conn_find_stream(conn, 0);

  assert_true(stream->flags & NGHTTP3_STREAM_FLAG_PRIORITY_UPDATE_RECVED);
  assert_uint32(1, ==, stream->node.pri.urgency);
  assert_uint8(0, ==, stream->node.pri.inc);

  nghttp3_conn_del(conn);

  /* Process PRIORITY_UPDATE frame that has priority field equal to
     buffer size; frame is received in 2 separate calls.  */
  nghttp3_buf_reset(&buf);
  setup_default_server(&conn);
  nghttp3_conn_set_max_client_streams_bidi(conn, 1);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  fr.hd.type = NGHTTP3_FRAME_SETTINGS;
  fr.settings.niv = 0;

  nghttp3_write_frame(&buf, (nghttp3_frame *)&fr);

  fr.hd.type = NGHTTP3_FRAME_PRIORITY_UPDATE;
  fr.priority_update.pri_elem_id = 0;
  fr.priority_update.data = (uint8_t *)"u=1,aaaa";
  fr.priority_update.datalen = strlen("u=1,aaaa");

  nghttp3_write_frame(&buf, (nghttp3_frame *)&fr);

  nconsumed =
    nghttp3_conn_read_stream(conn, 2, buf.pos, nghttp3_buf_len(&buf) - 1,
                             /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)(nghttp3_buf_len(&buf) - 1), ==, nconsumed);

  nconsumed =
    nghttp3_conn_read_stream(conn, 2, buf.pos + nghttp3_buf_len(&buf) - 1, 1,
                             /* fin = */ 0);

  assert_ptrdiff(1, ==, nconsumed);

  stream = nghttp3_conn_find_stream(conn, 0);

  assert_true(stream->flags & NGHTTP3_STREAM_FLAG_PRIORITY_UPDATE_RECVED);
  assert_uint32(1, ==, stream->node.pri.urgency);
  assert_uint8(0, ==, stream->node.pri.inc);

  nghttp3_conn_del(conn);

  /* PRIORITY_UPDATE frame contains invalid priority field. */
  nghttp3_buf_reset(&buf);
  setup_default_server(&conn);
  nghttp3_conn_set_max_client_streams_bidi(conn, 1);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  fr.hd.type = NGHTTP3_FRAME_SETTINGS;
  fr.settings.niv = 0;

  nghttp3_write_frame(&buf, (nghttp3_frame *)&fr);

  fr.hd.type = NGHTTP3_FRAME_PRIORITY_UPDATE;
  fr.priority_update.pri_elem_id = 0;
  fr.priority_update.data = (uint8_t *)"u=1,9x";
  fr.priority_update.datalen = strlen("u=1,9x");

  nghttp3_write_frame(&buf, (nghttp3_frame *)&fr);

  nconsumed = nghttp3_conn_read_stream(conn, 2, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff(NGHTTP3_ERR_H3_GENERAL_PROTOCOL_ERROR, ==, nconsumed);

  stream = nghttp3_conn_find_stream(conn, 0);

  assert_null(stream);

  nghttp3_conn_del(conn);

  /* Client receives PRIORITY_UPDATE frame */
  nghttp3_buf_reset(&buf);
  setup_default_client(&conn);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  fr.hd.type = NGHTTP3_FRAME_SETTINGS;
  fr.settings.niv = 0;

  nghttp3_write_frame(&buf, (nghttp3_frame *)&fr);

  fr.hd.type = NGHTTP3_FRAME_PRIORITY_UPDATE;
  fr.priority_update.pri_elem_id = 0;
  fr.priority_update.datalen = 0;

  nghttp3_write_frame(&buf, (nghttp3_frame *)&fr);

  nconsumed = nghttp3_conn_read_stream(conn, 3, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff(NGHTTP3_ERR_H3_FRAME_UNEXPECTED, ==, nconsumed);

  nghttp3_conn_del(conn);

  /* Receive empty PRIORITY_UPDATE frame */
  nghttp3_buf_reset(&buf);
  setup_default_server(&conn);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  fr.hd.type = NGHTTP3_FRAME_SETTINGS;
  fr.settings.niv = 0;

  nghttp3_write_frame(&buf, (nghttp3_frame *)&fr);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_FRAME_PRIORITY_UPDATE);
  buf.last = nghttp3_put_varint(buf.last, 0);

  nconsumed = nghttp3_conn_read_stream(conn, 2, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff(NGHTTP3_ERR_H3_FRAME_ERROR, ==, nconsumed);

  nghttp3_conn_del(conn);

  /* Receive PRIORITY_UPDATE frame in 1 byte at a time */
  nghttp3_buf_reset(&buf);
  setup_default_server(&conn);
  nghttp3_conn_set_max_client_streams_bidi(conn, 129);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  fr.hd.type = NGHTTP3_FRAME_SETTINGS;
  fr.settings.niv = 0;

  nghttp3_write_frame(&buf, (nghttp3_frame *)&fr);

  fr.hd.type = NGHTTP3_FRAME_PRIORITY_UPDATE;
  fr.priority_update.pri_elem_id = 512;
  fr.priority_update.data = (uint8_t *)"u=1";
  fr.priority_update.datalen = strlen("u=1");

  nghttp3_write_frame(&buf, (nghttp3_frame *)&fr);

  for (i = 0; i < nghttp3_buf_len(&buf); ++i) {
    nconsumed = nghttp3_conn_read_stream(conn, 2, buf.pos + i, 1,
                                         /* fin = */ 0);

    assert_ptrdiff(1, ==, nconsumed);
  }

  stream = nghttp3_conn_find_stream(conn, 2);

  assert_int(NGHTTP3_CTRL_STREAM_STATE_FRAME_TYPE, ==, stream->rstate.state);

  stream = nghttp3_conn_find_stream(conn, 512);

  assert_true(stream->flags & NGHTTP3_STREAM_FLAG_PRIORITY_UPDATE_RECVED);
  assert_uint32(1, ==, stream->node.pri.urgency);
  assert_uint8(0, ==, stream->node.pri.inc);
  nghttp3_conn_del(conn);
}

void test_nghttp3_conn_request_priority(void) {
  const nghttp3_mem *mem = nghttp3_mem_default();
  nghttp3_conn *conn;
  nghttp3_frame fr;
  nghttp3_ssize nconsumed;
  uint8_t rawbuf[2048];
  nghttp3_buf buf;
  nghttp3_stream *stream;
  nghttp3_qpack_encoder qenc;
  const nghttp3_nv nva[] = {
    MAKE_NV(":path", "/"),         MAKE_NV(":authority", "example.com"),
    MAKE_NV(":scheme", "https"),   MAKE_NV(":method", "GET"),
    MAKE_NV("priority", "u=5, i"),
  };
  const nghttp3_nv badpri_nva[] = {
    MAKE_NV(":path", "/"),         MAKE_NV(":authority", "example.com"),
    MAKE_NV(":scheme", "https"),   MAKE_NV(":method", "GET"),
    MAKE_NV("priority", "u=5, i"), MAKE_NV("priority", "i, u=x"),
  };

  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));

  /* Priority in request */
  setup_default_server(&conn);
  nghttp3_conn_set_max_client_streams_bidi(conn, 1);
  nghttp3_qpack_encoder_init(&qenc, 0, mem);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  fr.hd.type = NGHTTP3_FRAME_SETTINGS;
  fr.settings.niv = 0;

  nghttp3_write_frame(&buf, (nghttp3_frame *)&fr);

  nconsumed = nghttp3_conn_read_stream(conn, 2, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, nconsumed);

  nghttp3_buf_reset(&buf);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.headers.nva = (nghttp3_nv *)nva;
  fr.headers.nvlen = nghttp3_arraylen(nva);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, &fr);

  nconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 1);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, nconsumed);

  stream = nghttp3_conn_find_stream(conn, 0);

  assert_not_null(stream);
  assert_uint32(5, ==, stream->node.pri.urgency);
  assert_uint8(1, ==, stream->node.pri.inc);

  nghttp3_qpack_encoder_free(&qenc);
  nghttp3_conn_del(conn);

  /* Bad priority in request */
  nghttp3_buf_reset(&buf);
  setup_default_server(&conn);
  nghttp3_conn_set_max_client_streams_bidi(conn, 1);
  nghttp3_qpack_encoder_init(&qenc, 0, mem);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  fr.hd.type = NGHTTP3_FRAME_SETTINGS;
  fr.settings.niv = 0;

  nghttp3_write_frame(&buf, (nghttp3_frame *)&fr);

  nconsumed = nghttp3_conn_read_stream(conn, 2, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, nconsumed);

  nghttp3_buf_reset(&buf);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.headers.nva = (nghttp3_nv *)badpri_nva;
  fr.headers.nvlen = nghttp3_arraylen(badpri_nva);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, &fr);

  nconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 1);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, nconsumed);

  stream = nghttp3_conn_find_stream(conn, 0);

  assert_not_null(stream);
  assert_uint32(NGHTTP3_DEFAULT_URGENCY, ==, stream->node.pri.urgency);
  assert_uint8(0, ==, stream->node.pri.inc);

  nghttp3_qpack_encoder_free(&qenc);
  nghttp3_conn_del(conn);
}

void test_nghttp3_conn_set_stream_priority(void) {
  nghttp3_conn *conn;
  int rv;
  nghttp3_pri pri;
  nghttp3_frame_entry *ent;
  nghttp3_stream *stream;
  size_t i;

  /* Update stream priority by client */
  setup_default_client(&conn);

  rv = nghttp3_conn_submit_request(conn, 0, req_nva, nghttp3_arraylen(req_nva),
                                   NULL, NULL);

  assert_int(0, ==, rv);

  pri.urgency = 2;
  pri.inc = 1;

#define NGHTTP3_PRI_DATA "u=2,i"
  rv = nghttp3_conn_set_client_stream_priority(
    conn, 0, (const uint8_t *)NGHTTP3_PRI_DATA, strlen(NGHTTP3_PRI_DATA));

  assert_int(0, ==, rv);

  stream = nghttp3_conn_find_stream(conn, 2);

  for (i = 0; i < nghttp3_ringbuf_len(&stream->frq); ++i) {
    ent = nghttp3_ringbuf_get(&stream->frq, i);
    if (ent->fr.hd.type != NGHTTP3_FRAME_PRIORITY_UPDATE) {
      continue;
    }

    assert_size(strlen(NGHTTP3_PRI_DATA), ==, ent->fr.priority_update.datalen);
    assert_memory_equal(strlen(NGHTTP3_PRI_DATA), NGHTTP3_PRI_DATA,
                        ent->fr.priority_update.data);

    break;
  }
#undef NGHTTP3_PRI_DATA

  assert_size(nghttp3_ringbuf_len(&stream->frq), >, i);

  nghttp3_conn_del(conn);

  /* Updating priority of stream which does not exist is an error */
  setup_default_client(&conn);

  pri.urgency = 2;
  pri.inc = 1;

#define NGHTTP3_PRI_DATA "u=2,i"
  rv = nghttp3_conn_set_client_stream_priority(
    conn, 0, (const uint8_t *)NGHTTP3_PRI_DATA, strlen(NGHTTP3_PRI_DATA));
#undef NGHTTP3_PRI_DATA

  assert_int(NGHTTP3_ERR_STREAM_NOT_FOUND, ==, rv);

  nghttp3_conn_del(conn);

  /* Update stream priority by server */
  setup_default_server(&conn);

  rv = nghttp3_conn_create_stream(conn, &stream, 0);

  assert_int(0, ==, rv);

  pri.urgency = 4;
  pri.inc = 0;

  rv = nghttp3_conn_set_server_stream_priority(conn, 0, &pri);

  assert_int(0, ==, rv);

  stream = nghttp3_conn_find_stream(conn, 0);

  assert_true(stream->flags & NGHTTP3_STREAM_FLAG_SERVER_PRIORITY_SET);
  assert_true(nghttp3_pri_eq(&pri, &stream->node.pri));

  nghttp3_conn_del(conn);
}

void test_nghttp3_conn_shutdown_stream_read(void) {
  const nghttp3_mem *mem = nghttp3_mem_default();
  nghttp3_conn *conn;
  nghttp3_callbacks callbacks = {
    .deferred_consume = deferred_consume,
  };
  nghttp3_settings settings;
  nghttp3_qpack_encoder qenc;
  int rv;
  nghttp3_buf ebuf;
  uint8_t rawbuf[4096];
  nghttp3_buf buf;
  nghttp3_frame fr;
  nghttp3_ssize sconsumed;
  size_t consumed_total;
  userdata ud;
  size_t indatalen;
  conn_options opts;

  nghttp3_settings_default(&settings);
  settings.qpack_max_dtable_capacity = 4096;
  settings.qpack_blocked_streams = 100;

  /* Shutting down read-side stream when a stream is blocked by QPACK
     dependency. */
  nghttp3_buf_init(&ebuf);
  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));

  nghttp3_qpack_encoder_init(&qenc, settings.qpack_max_dtable_capacity, mem);
  nghttp3_qpack_encoder_set_max_blocked_streams(&qenc,
                                                settings.qpack_blocked_streams);
  nghttp3_qpack_encoder_set_max_dtable_capacity(
    &qenc, settings.qpack_max_dtable_capacity);

  conn_options_clear(&opts);
  opts.callbacks = &callbacks;
  opts.settings = &settings;
  opts.user_data = &ud;

  setup_default_client_with_options(&conn, opts);

  rv = nghttp3_conn_submit_request(conn, 0, req_nva, nghttp3_arraylen(req_nva),
                                   NULL, NULL);

  assert_int(0, ==, rv);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.headers.nva = (nghttp3_nv *)resp_nva;
  fr.headers.nvlen = nghttp3_arraylen(resp_nva);

  nghttp3_write_frame_qpack_dyn(&buf, &ebuf, &qenc, 0, &fr);

  indatalen = nghttp3_buf_len(&buf);

  ud.deferred_consume_cb.consumed_total = 0;
  consumed_total = 0;
  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff(0, <, sconsumed);
  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), !=, sconsumed);

  consumed_total += (size_t)sconsumed;

  rv = nghttp3_conn_shutdown_stream_read(conn, 0);

  assert_int(0, ==, rv);
  assert_size(1, ==, nghttp3_buf_len(&conn->qdec.dbuf));

  /* Reading further stream data is discarded. */
  nghttp3_buf_reset(&buf);
  *buf.pos = 0;
  ++buf.last;

  ++indatalen;

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 1);

  assert_ptrdiff(1, ==, sconsumed);

  consumed_total += (size_t)sconsumed;

  nghttp3_buf_reset(&buf);
  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_QPACK_ENCODER);

  sconsumed = nghttp3_conn_read_stream(conn, 7, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, sconsumed);

  sconsumed = nghttp3_conn_read_stream(conn, 7, ebuf.pos,
                                       nghttp3_buf_len(&ebuf), /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&ebuf), ==, sconsumed);
  /* Make sure that Section Acknowledgement is not written. */
  assert_size(1, ==, nghttp3_buf_len(&conn->qdec.dbuf));
  assert_size(indatalen, ==,
              consumed_total + ud.deferred_consume_cb.consumed_total);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);
  nghttp3_buf_free(&ebuf, mem);
}

void test_nghttp3_conn_stream_data_overflow(void) {
#if SIZE_MAX > UINT32_MAX
  nghttp3_conn *conn;
  nghttp3_vec vec[256];
  nghttp3_ssize sveccnt;
  int rv;
  int64_t stream_id;
  nghttp3_data_reader dr;
  int fin;

  /* Specify NGHTTP3_MAX_VARINT + 1 bytes data in
     nghttp3_read_data_callback. */
  setup_default_client(&conn);
  conn_write_initial_streams(conn);

  dr.read_data = stream_data_overflow_read_data;

  rv = nghttp3_conn_submit_request(conn, 0, req_nva, nghttp3_arraylen(req_nva),
                                   &dr, NULL);

  assert_int(0, ==, rv);

  /* Write request stream */
  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  assert_ptrdiff(NGHTTP3_ERR_STREAM_DATA_OVERFLOW, ==, sveccnt);

  nghttp3_conn_del(conn);

  /* nghttp3_stream_outq_add detects stream data overflow */
  setup_default_client(&conn);
  conn_write_initial_streams(conn);

  dr.read_data = stream_data_almost_overflow_read_data;

  rv = nghttp3_conn_submit_request(conn, 0, req_nva, nghttp3_arraylen(req_nva),
                                   &dr, NULL);

  assert_int(0, ==, rv);

  /* Write request stream */
  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  assert_ptrdiff(NGHTTP3_ERR_STREAM_DATA_OVERFLOW, ==, sveccnt);

  nghttp3_conn_del(conn);
#endif /* SIZE_MAX > UINT32_MAX */
}

void test_nghttp3_conn_get_frame_payload_left(void) {
  const nghttp3_mem *mem = nghttp3_mem_default();
  nghttp3_conn *conn;
  struct {
    nghttp3_frame_settings settings;
    nghttp3_settings_entry iv[3];
  } settingsfr;
  nghttp3_frame fr;
  uint8_t rawbuf[1024];
  nghttp3_buf buf;
  nghttp3_settings_entry *iv;
  nghttp3_ssize nconsumed;
  nghttp3_qpack_encoder qenc;

  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));

  /* Control stream */
  setup_default_server(&conn);

  assert_uint64(0, ==, nghttp3_conn_get_frame_payload_left(conn, 2));

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  settingsfr.settings.hd.type = NGHTTP3_FRAME_SETTINGS;
  iv = settingsfr.settings.iv;
  iv[0].id = NGHTTP3_SETTINGS_ID_MAX_FIELD_SECTION_SIZE;
  iv[0].value = 1000000009;
  iv[1].id = NGHTTP3_SETTINGS_ID_QPACK_MAX_TABLE_CAPACITY;
  iv[1].value = 1000000007;
  iv[2].id = NGHTTP3_SETTINGS_ID_QPACK_BLOCKED_STREAMS;
  iv[2].value = 1000000001;
  settingsfr.settings.niv = 3;

  nghttp3_write_frame(&buf, (nghttp3_frame *)&settingsfr);

  assert_size(18, ==, nghttp3_buf_len(&buf));

  nconsumed = nghttp3_conn_read_stream(conn, 2, buf.pos, 3, /* fin = */ 0);

  assert_ptrdiff(3, ==, nconsumed);
  assert_uint64(nghttp3_buf_len(&buf) - 3, ==,
                nghttp3_conn_get_frame_payload_left(conn, 2));

  nconsumed = nghttp3_conn_read_stream(conn, 2, buf.pos + 3, 14, /* fin = */ 0);

  assert_ptrdiff(14, ==, nconsumed);
  assert_uint64(nghttp3_buf_len(&buf) - 17, ==,
                nghttp3_conn_get_frame_payload_left(conn, 2));

  nconsumed = nghttp3_conn_read_stream(conn, 2, buf.pos + 17, 1, /* fin = */ 0);

  assert_ptrdiff(1, ==, nconsumed);
  assert_uint64(0, ==, nghttp3_conn_get_frame_payload_left(conn, 2));

  nghttp3_conn_del(conn);

  /* Client bidi stream */
  nghttp3_buf_reset(&buf);
  setup_default_server(&conn);

  assert_uint64(0, ==, nghttp3_conn_get_frame_payload_left(conn, 0));

  nghttp3_qpack_encoder_init(&qenc, 0, mem);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.headers.nva = (nghttp3_nv *)req_nva;
  fr.headers.nvlen = nghttp3_arraylen(req_nva);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, (nghttp3_frame *)&fr);

  nconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, 1, /* fin = 0 */ 0);

  assert_ptrdiff(1, ==, nconsumed);
  assert_uint64(0, ==, nghttp3_conn_get_frame_payload_left(conn, 0));

  nconsumed =
    nghttp3_conn_read_stream(conn, 0, buf.pos + 1, 1, /* fin = 0 */ 0);

  assert_ptrdiff(1, ==, nconsumed);
  assert_uint64(nghttp3_buf_len(&buf) - 2, ==,
                nghttp3_conn_get_frame_payload_left(conn, 0));

  nghttp3_qpack_encoder_free(&qenc);
  nghttp3_conn_del(conn);
}

void test_nghttp3_conn_update_ack_offset(void) {
  nghttp3_conn *conn;
  nghttp3_callbacks callbacks = {
    .acked_stream_data = acked_stream_data,
  };
  nghttp3_vec vec[256];
  nghttp3_ssize sveccnt;
  int rv;
  int64_t stream_id;
  uint64_t len;
  nghttp3_stream *stream;
  userdata ud = {0};
  nghttp3_data_reader dr;
  int fin;
  uint64_t ack_offset;
  conn_options opts;

  ud.data.left = 2000;
  ud.data.step = 1333;

  conn_options_clear(&opts);
  opts.callbacks = &callbacks;
  opts.user_data = &ud;

  setup_default_client_with_options(&conn, opts);
  conn_write_initial_streams(conn);

  dr.read_data = step_read_data;
  rv = nghttp3_conn_submit_request(conn, 0, req_nva, nghttp3_arraylen(req_nva),
                                   &dr, NULL);

  assert_int(0, ==, rv);

  /* This will write request stream */
  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  assert_int64(0, ==, stream_id);
  assert_ptrdiff(2, ==, sveccnt);

  len = nghttp3_vec_len(vec, (size_t)sveccnt);

  rv = nghttp3_conn_add_write_offset(conn, 0, (size_t)len);

  assert_int(0, ==, rv);

  stream = nghttp3_conn_find_stream(conn, 0);

  ack_offset = 100;

  rv = nghttp3_conn_update_ack_offset(conn, 0, ack_offset);

  assert_int(0, ==, rv);
  assert_uint64(ack_offset, ==, stream->ack_offset);

  ack_offset = len;

  rv = nghttp3_conn_update_ack_offset(conn, 0, ack_offset);

  assert_int(0, ==, rv);
  assert_size(0, ==, nghttp3_ringbuf_len(&stream->outq));
  assert_size(0, ==, nghttp3_ringbuf_len(&stream->chunks));
  assert_size(0, ==, stream->outq_idx);
  assert_uint64(ack_offset, ==, stream->ack_offset);

  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  assert_int64(0, ==, stream_id);
  assert_ptrdiff(2, ==, sveccnt);

  len = nghttp3_vec_len(vec, (size_t)sveccnt);

  rv = nghttp3_conn_add_write_offset(conn, 0, (size_t)len);

  assert_int(0, ==, rv);

  /* Make sure that we do not call acked_data with 0 length for an
     alien buffer. */
  ack_offset += vec[0].len;

  ud.ack.ncalled = 0;

  rv = nghttp3_conn_update_ack_offset(conn, 0, ack_offset);

  assert_int(0, ==, rv);
  assert_size(0, ==, ud.ack.ncalled);

  /* Check with the same offset. */
  rv = nghttp3_conn_update_ack_offset(conn, 0, ack_offset);

  assert_int(0, ==, rv);
  assert_size(0, ==, ud.ack.ncalled);

  /* Calling the function with smaller offset is an error. */
  rv = nghttp3_conn_update_ack_offset(conn, 0, ack_offset - 1);

  assert_int(NGHTTP3_ERR_INVALID_ARGUMENT, ==, rv);
  assert_uint64(ack_offset, ==, stream->ack_offset);

  nghttp3_conn_del(conn);
}

void test_nghttp3_conn_set_client_stream_priority(void) {
  nghttp3_conn *conn;
  static const uint8_t prihd[] = "u=0";
  nghttp3_vec vec[256];
  nghttp3_ssize sveccnt;
  int64_t stream_id;
  nghttp3_ssize nread;
  nghttp3_frame_priority_update fr;
  int fin;
  int rv;

  setup_default_client(&conn);
  conn_write_initial_streams(conn);

  rv = nghttp3_conn_submit_request(conn, 0, req_nva, nghttp3_arraylen(req_nva),
                                   NULL, NULL);

  assert_int(0, ==, rv);

  rv = nghttp3_conn_set_client_stream_priority(conn, 0, prihd, strsize(prihd));

  assert_int(0, ==, rv);

  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  assert_ptrdiff(1, ==, sveccnt);
  assert_int64(2, ==, stream_id);

  nread = nghttp3_decode_priority_update_frame(&fr, vec, (size_t)sveccnt);

  assert_ptrdiff((nghttp3_ssize)nghttp3_vec_len(vec, (size_t)sveccnt), ==,
                 nread);
  assert_int64(NGHTTP3_FRAME_PRIORITY_UPDATE, ==, fr.hd.type);
  assert_memn_equal(prihd, strsize(prihd), fr.data, fr.datalen);

  rv = nghttp3_conn_add_write_offset(
    conn, stream_id, (size_t)nghttp3_vec_len(vec, (size_t)sveccnt));

  assert_int(0, ==, rv);

  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  assert_ptrdiff(1, ==, sveccnt);
  assert_int64(0, ==, stream_id);

  nghttp3_conn_del(conn);
}

void test_nghttp3_conn_rx_http_state(void) {
  const nghttp3_mem *mem = nghttp3_mem_default();
  const nghttp3_nv req_connect_nva[] = {
    MAKE_NV(":method", "CONNECT"),
    MAKE_NV(":authority", "localhost:4433"),
  };
  const nghttp3_nv resp_not_found_nva[] = {
    MAKE_NV(":status", "404"),
  };
  const nghttp3_nv trailer_nva[] = {
    MAKE_NV("alpha", "bravo"),
  };
  nghttp3_conn *conn;
  uint8_t rawbuf[4096];
  nghttp3_buf buf;
  nghttp3_ssize sconsumed;
  nghttp3_qpack_encoder qenc;
  nghttp3_frame fr;
  nghttp3_vec vec[256];
  nghttp3_ssize sveccnt;
  nghttp3_stream *stream;
  int64_t stream_id;
  int fin;
  int rv;

  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));

  /* Server receives DATA before HEADERS */
  setup_default_server(&conn);

  nghttp3_write_frame_data(&buf, 7);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff(NGHTTP3_ERR_H3_FRAME_UNEXPECTED, ==, sconsumed);

  nghttp3_conn_del(conn);

  /* Server receives fin without completing HEADERS */
  nghttp3_buf_reset(&buf);
  nghttp3_qpack_encoder_init(&qenc, 0, mem);

  setup_default_server(&conn);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.headers.nva = (nghttp3_nv *)req_nva;
  fr.headers.nvlen = nghttp3_arraylen(req_nva);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, &fr);

  sconsumed =
    nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf) - 1,
                             /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf) - 1, ==, sconsumed);

  sconsumed = nghttp3_conn_read_stream(conn, 0, NULL, 0,
                                       /* fin = */ 1);

  assert_ptrdiff(NGHTTP3_ERR_H3_FRAME_ERROR, ==, sconsumed);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* Client sends CONNECT request, and gets non-2xx response with
     trailers. */
  nghttp3_buf_reset(&buf);
  nghttp3_qpack_encoder_init(&qenc, 0, mem);

  setup_default_client(&conn);
  conn_write_initial_streams(conn);

  rv = nghttp3_conn_submit_request(
    conn, 0, req_connect_nva, nghttp3_arraylen(req_connect_nva), NULL, NULL);

  assert_int(0, ==, rv);

  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  assert_ptrdiff(1, ==, sveccnt);
  assert_int64(0, ==, stream_id);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.headers.nva = (nghttp3_nv *)resp_not_found_nva;
  fr.headers.nvlen = nghttp3_arraylen(resp_not_found_nva);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, &fr);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, sconsumed);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.headers.nva = (nghttp3_nv *)trailer_nva;
  fr.headers.nvlen = nghttp3_arraylen(trailer_nva);

  nghttp3_buf_reset(&buf);
  nghttp3_write_frame_qpack(&buf, &qenc, 0, &fr);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 1);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, sconsumed);

  stream = nghttp3_conn_find_stream(conn, 0);

  assert_enum(nghttp3_stream_http_state, NGHTTP3_HTTP_STATE_RESP_END, ==,
              stream->rx.hstate);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* Server receives headers, data, and then trailers */
  nghttp3_buf_reset(&buf);
  nghttp3_qpack_encoder_init(&qenc, 0, mem);

  setup_default_server(&conn);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.headers.nva = (nghttp3_nv *)req_nva;
  fr.headers.nvlen = nghttp3_arraylen(req_nva);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, &fr);
  nghttp3_write_frame_data(&buf, 11);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf) - 11, ==, sconsumed);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.headers.nva = (nghttp3_nv *)trailer_nva;
  fr.headers.nvlen = nghttp3_arraylen(trailer_nva);

  nghttp3_buf_reset(&buf);
  nghttp3_write_frame_qpack(&buf, &qenc, 0, &fr);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 1);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, sconsumed);

  stream = nghttp3_conn_find_stream(conn, 0);

  assert_enum(nghttp3_stream_http_state, NGHTTP3_HTTP_STATE_REQ_END, ==,
              stream->rx.hstate);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* Client receives headers and fin. */
  nghttp3_buf_reset(&buf);
  nghttp3_qpack_encoder_init(&qenc, 0, mem);

  setup_default_client(&conn);
  conn_write_initial_streams(conn);

  rv = nghttp3_conn_submit_request(conn, 0, req_nva, nghttp3_arraylen(req_nva),
                                   NULL, NULL);

  assert_int(0, ==, rv);

  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  assert_ptrdiff(1, ==, sveccnt);
  assert_int64(0, ==, stream_id);

  rv = nghttp3_conn_add_write_offset(
    conn, stream_id, (size_t)nghttp3_vec_len(vec, (size_t)sveccnt));

  assert_int(0, ==, rv);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.headers.nva = (nghttp3_nv *)resp_not_found_nva;
  fr.headers.nvlen = nghttp3_arraylen(resp_not_found_nva);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, &fr);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 1);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, sconsumed);

  stream = nghttp3_conn_find_stream(conn, 0);

  assert_enum(nghttp3_stream_http_state, NGHTTP3_HTTP_STATE_RESP_END, ==,
              stream->rx.hstate);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* Client receives DATA before HEADERS. */
  nghttp3_buf_reset(&buf);
  setup_default_client(&conn);
  conn_write_initial_streams(conn);

  rv = nghttp3_conn_submit_request(conn, 0, req_nva, nghttp3_arraylen(req_nva),
                                   NULL, NULL);

  assert_int(0, ==, rv);

  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  assert_ptrdiff(1, ==, sveccnt);
  assert_int64(0, ==, stream_id);

  rv = nghttp3_conn_add_write_offset(
    conn, stream_id, (size_t)nghttp3_vec_len(vec, (size_t)sveccnt));

  assert_int(0, ==, rv);

  nghttp3_write_frame_data(&buf, 119);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff(NGHTTP3_ERR_H3_FRAME_UNEXPECTED, ==, sconsumed);

  nghttp3_conn_del(conn);

  /* Client receives headers, data, and then trailers. */
  nghttp3_buf_reset(&buf);
  nghttp3_qpack_encoder_init(&qenc, 0, mem);

  setup_default_client(&conn);
  conn_write_initial_streams(conn);

  rv = nghttp3_conn_submit_request(conn, 0, req_nva, nghttp3_arraylen(req_nva),
                                   NULL, NULL);

  assert_int(0, ==, rv);

  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  assert_ptrdiff(1, ==, sveccnt);
  assert_int64(0, ==, stream_id);

  rv = nghttp3_conn_add_write_offset(
    conn, stream_id, (size_t)nghttp3_vec_len(vec, (size_t)sveccnt));

  assert_int(0, ==, rv);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.headers.nva = (nghttp3_nv *)resp_not_found_nva;
  fr.headers.nvlen = nghttp3_arraylen(resp_not_found_nva);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, &fr);
  nghttp3_write_frame_data(&buf, 73);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf) - 73, ==, sconsumed);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.headers.nva = (nghttp3_nv *)trailer_nva;
  fr.headers.nvlen = nghttp3_arraylen(trailer_nva);

  nghttp3_buf_reset(&buf);
  nghttp3_write_frame_qpack(&buf, &qenc, 0, &fr);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 1);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, sconsumed);

  stream = nghttp3_conn_find_stream(conn, 0);

  assert_enum(nghttp3_stream_http_state, NGHTTP3_HTTP_STATE_RESP_END, ==,
              stream->rx.hstate);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);

  /* Server receives empty headers */
  nghttp3_buf_reset(&buf);
  setup_default_server(&conn);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.hd.length = 0;

  buf.last = nghttp3_frame_write_hd(buf.last, &fr.hd);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff(NGHTTP3_ERR_MALFORMED_HTTP_MESSAGING, ==, sconsumed);

  nghttp3_conn_del(conn);

  /* Server receives headers, data, and then empty trailers */
  nghttp3_buf_reset(&buf);
  nghttp3_qpack_encoder_init(&qenc, 0, mem);

  setup_default_server(&conn);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.headers.nva = (nghttp3_nv *)req_nva;
  fr.headers.nvlen = nghttp3_arraylen(req_nva);

  nghttp3_write_frame_qpack(&buf, &qenc, 0, &fr);
  nghttp3_write_frame_data(&buf, 999);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf) - 999, ==, sconsumed);

  fr.hd.type = NGHTTP3_FRAME_HEADERS;
  fr.hd.length = 0;

  nghttp3_buf_reset(&buf);
  buf.last = nghttp3_frame_write_hd(buf.last, &fr.hd);

  sconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 1);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, sconsumed);

  stream = nghttp3_conn_find_stream(conn, 0);

  assert_enum(nghttp3_stream_http_state, NGHTTP3_HTTP_STATE_REQ_END, ==,
              stream->rx.hstate);

  nghttp3_conn_del(conn);
  nghttp3_qpack_encoder_free(&qenc);
}

void test_nghttp3_conn_push(void) {
  nghttp3_conn *conn;
  uint8_t rawbuf[4096];
  nghttp3_buf buf;
  nghttp3_ssize nconsumed;
  nghttp3_stream *stream;
  nghttp3_frame fr = {
    .settings =
      {
        .hd =
          {
            .type = NGHTTP3_FRAME_SETTINGS,
          },
        .niv = 0,
      },
  };
  int fin;
  nghttp3_vec vec[256];
  nghttp3_ssize sveccnt;
  int rv;
  int64_t stream_id;
  size_t i;

  nghttp3_buf_wrap_init(&buf, rawbuf, sizeof(rawbuf));

  /* MAX_PUSH_ID from client is ignored. */
  setup_default_server(&conn);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  nghttp3_write_frame(&buf, &fr);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_FRAME_MAX_PUSH_ID);
  buf.last = nghttp3_put_varint(buf.last, (int64_t)nghttp3_put_varintlen(64));
  buf.last = nghttp3_put_varint(buf.last, 64);

  nconsumed = nghttp3_conn_read_stream(conn, 2, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff((nghttp3_ssize)nghttp3_buf_len(&buf), ==, nconsumed);

  stream = nghttp3_conn_find_stream(conn, 2);

  assert_int(NGHTTP3_CTRL_STREAM_STATE_FRAME_TYPE, ==, stream->rstate.state);

  nghttp3_conn_del(conn);

  /* Receive MAX_PUSH_ID frame in 1 byte at a time. */
  nghttp3_buf_reset(&buf);
  setup_default_server(&conn);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  nghttp3_write_frame(&buf, &fr);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_FRAME_MAX_PUSH_ID);
  buf.last = nghttp3_put_varint(buf.last, (int64_t)nghttp3_put_varintlen(64));
  buf.last = nghttp3_put_varint(buf.last, 64);

  for (i = 0; i < nghttp3_buf_len(&buf); ++i) {
    nconsumed = nghttp3_conn_read_stream(conn, 2, buf.pos + i, 1,
                                         /* fin = */ 0);

    assert_ptrdiff(1, ==, nconsumed);
  }

  stream = nghttp3_conn_find_stream(conn, 2);

  assert_int(NGHTTP3_CTRL_STREAM_STATE_FRAME_TYPE, ==, stream->rstate.state);

  nghttp3_conn_del(conn);

  /* Receiving smaller MAX_PUSH_ID from client is treated as error. */
  nghttp3_buf_reset(&buf);
  setup_default_server(&conn);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  nghttp3_write_frame(&buf, &fr);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_FRAME_MAX_PUSH_ID);
  buf.last = nghttp3_put_varint(buf.last, (int64_t)nghttp3_put_varintlen(64));
  buf.last = nghttp3_put_varint(buf.last, 64);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_FRAME_MAX_PUSH_ID);
  buf.last = nghttp3_put_varint(buf.last, (int64_t)nghttp3_put_varintlen(63));
  buf.last = nghttp3_put_varint(buf.last, 63);

  nconsumed = nghttp3_conn_read_stream(conn, 2, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff(NGHTTP3_ERR_H3_FRAME_ERROR, ==, nconsumed);

  nghttp3_conn_del(conn);

  /* Receiving invalid MAX_PUSH_ID from client is treated as error. */
  nghttp3_buf_reset(&buf);
  setup_default_server(&conn);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  nghttp3_write_frame(&buf, &fr);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_FRAME_MAX_PUSH_ID);
  buf.last = nghttp3_put_varint(buf.last, 0);

  nconsumed = nghttp3_conn_read_stream(conn, 2, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff(NGHTTP3_ERR_H3_FRAME_ERROR, ==, nconsumed);

  nghttp3_conn_del(conn);

  /* Receiving MAX_PUSH_ID from server is treated as error. */
  nghttp3_buf_reset(&buf);
  setup_default_client(&conn);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  nghttp3_write_frame(&buf, &fr);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_FRAME_MAX_PUSH_ID);
  buf.last = nghttp3_put_varint(buf.last, (int64_t)nghttp3_put_varintlen(64));
  buf.last = nghttp3_put_varint(buf.last, 64);

  nconsumed = nghttp3_conn_read_stream(conn, 3, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff(NGHTTP3_ERR_H3_FRAME_UNEXPECTED, ==, nconsumed);

  nghttp3_conn_del(conn);

  /* Receiving invalid CANCEL_PUSH from client is treated as error. */
  nghttp3_buf_reset(&buf);
  setup_default_server(&conn);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_STREAM_TYPE_CONTROL);

  nghttp3_write_frame(&buf, &fr);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_FRAME_CANCEL_PUSH);
  buf.last = nghttp3_put_varint(buf.last, (int64_t)nghttp3_put_varintlen(0));
  buf.last = nghttp3_put_varint(buf.last, 0);

  nconsumed = nghttp3_conn_read_stream(conn, 2, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff(NGHTTP3_ERR_H3_FRAME_UNEXPECTED, ==, nconsumed);

  nghttp3_conn_del(conn);

  /* Receiving PUSH_PROMISE from server is treated as error. */
  nghttp3_buf_reset(&buf);
  setup_default_client(&conn);
  conn_write_initial_streams(conn);

  rv = nghttp3_conn_submit_request(conn, 0, req_nva, nghttp3_arraylen(req_nva),
                                   NULL, NULL);

  assert_int(0, ==, rv);

  sveccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec,
                                       nghttp3_arraylen(vec));

  assert_int64(0, ==, stream_id);
  assert_ptrdiff(1, ==, sveccnt);

  buf.last = nghttp3_put_varint(buf.last, NGHTTP3_FRAME_PUSH_PROMISE);
  buf.last = nghttp3_put_varint(buf.last, (int64_t)nghttp3_put_varintlen(107));

  nconsumed = nghttp3_conn_read_stream(conn, 0, buf.pos, nghttp3_buf_len(&buf),
                                       /* fin = */ 0);

  assert_ptrdiff(NGHTTP3_ERR_H3_FRAME_UNEXPECTED, ==, nconsumed);

  nghttp3_conn_del(conn);
}
