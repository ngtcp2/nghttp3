/*
 * nghttp3
 *
 * Copyright (c) 2025 nghttp3 contributors
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
#include "nghttp3_callbacks_test.h"

#include <stdio.h>

#include "nghttp3_callbacks.h"
#include "nghttp3_test_helper.h"

static const MunitTest tests[] = {
  munit_void_test(test_nghttp3_callbacks_convert_to_latest),
  munit_void_test(test_nghttp3_callbacks_convert_to_old),
  munit_test_end(),
};

const MunitSuite callbacks_suite = {
  .prefix = "/callbacks",
  .tests = tests,
};

static int acked_stream_data(nghttp3_conn *conn, int64_t stream_id,
                             uint64_t datalen, void *conn_user_data,
                             void *stream_user_data) {
  (void)conn;
  (void)stream_id;
  (void)datalen;
  (void)conn_user_data;
  (void)stream_user_data;

  return 0;
}

static int stream_close(nghttp3_conn *conn, int64_t stream_id,
                        uint64_t app_error_code, void *conn_user_data,
                        void *stream_user_data) {
  (void)conn;
  (void)stream_id;
  (void)app_error_code;
  (void)conn_user_data;
  (void)stream_user_data;

  return 0;
}

static int recv_data(nghttp3_conn *conn, int64_t stream_id, const uint8_t *data,
                     size_t datalen, void *conn_user_data,
                     void *stream_user_data) {
  (void)conn;
  (void)stream_id;
  (void)data;
  (void)datalen;
  (void)conn_user_data;
  (void)stream_user_data;

  return 0;
}

static int deferred_consume(nghttp3_conn *conn, int64_t stream_id,
                            size_t consumed, void *conn_user_data,
                            void *stream_user_data) {
  (void)conn;
  (void)stream_id;
  (void)consumed;
  (void)conn_user_data;
  (void)stream_user_data;

  return 0;
}

static int begin_headers(nghttp3_conn *conn, int64_t stream_id,
                         void *conn_user_data, void *stream_user_data) {
  (void)conn;
  (void)stream_id;
  (void)conn_user_data;
  (void)stream_user_data;

  return 0;
}

static int recv_header(nghttp3_conn *conn, int64_t stream_id, int32_t token,
                       nghttp3_rcbuf *name, nghttp3_rcbuf *value, uint8_t flags,
                       void *conn_user_data, void *stream_user_data) {
  (void)conn;
  (void)stream_id;
  (void)token;
  (void)name;
  (void)value;
  (void)flags;
  (void)conn_user_data;
  (void)stream_user_data;

  return 0;
}

static int end_headers(nghttp3_conn *conn, int64_t stream_id, int fin,
                       void *conn_user_data, void *stream_user_data) {
  (void)conn;
  (void)stream_id;
  (void)fin;
  (void)conn_user_data;
  (void)stream_user_data;

  return 0;
}

static int begin_trailers(nghttp3_conn *conn, int64_t stream_id,
                          void *conn_user_data, void *stream_user_data) {
  (void)conn;
  (void)stream_id;
  (void)conn_user_data;
  (void)stream_user_data;

  return 0;
}

static int recv_trailer(nghttp3_conn *conn, int64_t stream_id, int32_t token,
                        nghttp3_rcbuf *name, nghttp3_rcbuf *value,
                        uint8_t flags, void *conn_user_data,
                        void *stream_user_data) {
  (void)conn;
  (void)stream_id;
  (void)token;
  (void)name;
  (void)value;
  (void)flags;
  (void)conn_user_data;
  (void)stream_user_data;

  return 0;
}

static int end_trailers(nghttp3_conn *conn, int64_t stream_id, int fin,
                        void *conn_user_data, void *stream_user_data) {
  (void)conn;
  (void)stream_id;
  (void)fin;
  (void)conn_user_data;
  (void)stream_user_data;

  return 0;
}

static int stop_sending(nghttp3_conn *conn, int64_t stream_id,
                        uint64_t app_error_code, void *conn_user_data,
                        void *stream_user_data) {
  (void)conn;
  (void)stream_id;
  (void)app_error_code;
  (void)conn_user_data;
  (void)stream_user_data;

  return 0;
}

static int end_stream(nghttp3_conn *conn, int64_t stream_id,
                      void *conn_user_data, void *stream_user_data) {
  (void)conn;
  (void)stream_id;
  (void)conn_user_data;
  (void)stream_user_data;

  return 0;
}

static int reset_stream(nghttp3_conn *conn, int64_t stream_id,
                        uint64_t app_error_code, void *conn_user_data,
                        void *stream_user_data) {
  (void)conn;
  (void)stream_id;
  (void)app_error_code;
  (void)conn_user_data;
  (void)stream_user_data;

  return 0;
}

static int shutdown(nghttp3_conn *conn, int64_t id, void *conn_user_data) {
  (void)conn;
  (void)id;
  (void)conn_user_data;

  return 0;
}

static int recv_settings(nghttp3_conn *conn, const nghttp3_settings *settings,
                         void *conn_user_data) {
  (void)conn;
  (void)settings;
  (void)conn_user_data;

  return 0;
}

static int recv_origin(nghttp3_conn *conn, const uint8_t *origin,
                       size_t originlen, void *conn_user_data) {
  (void)conn;
  (void)origin;
  (void)originlen;
  (void)conn_user_data;

  return 0;
}

static int end_origin(nghttp3_conn *conn, void *conn_user_data) {
  (void)conn;
  (void)conn_user_data;

  return 0;
}

void test_nghttp3_callbacks_convert_to_latest(void) {
  nghttp3_callbacks *src, srcbuf, callbacksbuf;
  const nghttp3_callbacks *dest;
  size_t v1len;

  memset(&srcbuf, 0, sizeof(srcbuf));

  srcbuf.acked_stream_data = acked_stream_data;
  srcbuf.stream_close = stream_close;
  srcbuf.recv_data = recv_data;
  srcbuf.deferred_consume = deferred_consume;
  srcbuf.begin_headers = begin_headers;
  srcbuf.recv_header = recv_header;
  srcbuf.end_headers = end_headers;
  srcbuf.begin_trailers = begin_trailers;
  srcbuf.recv_trailer = recv_trailer;
  srcbuf.end_trailers = end_trailers;
  srcbuf.stop_sending = stop_sending;
  srcbuf.end_stream = end_stream;
  srcbuf.reset_stream = reset_stream;
  srcbuf.shutdown = shutdown;
  srcbuf.recv_settings = recv_settings;

  v1len = nghttp3_callbackslen_version(NGHTTP3_CALLBACKS_V1);

  src = malloc(v1len);

  memcpy(src, &srcbuf, v1len);

  dest = nghttp3_callbacks_convert_to_latest(&callbacksbuf,
                                             NGHTTP3_CALLBACKS_V1, src);

  free(src);

  assert_ptr_equal(&callbacksbuf, dest);
  assert_ptr_equal(srcbuf.acked_stream_data, dest->acked_stream_data);
  assert_ptr_equal(srcbuf.stream_close, dest->stream_close);
  assert_ptr_equal(srcbuf.recv_data, dest->recv_data);
  assert_ptr_equal(srcbuf.deferred_consume, dest->deferred_consume);
  assert_ptr_equal(srcbuf.begin_headers, dest->begin_headers);
  assert_ptr_equal(srcbuf.recv_header, dest->recv_header);
  assert_ptr_equal(srcbuf.end_headers, dest->end_headers);
  assert_ptr_equal(srcbuf.begin_trailers, dest->begin_trailers);
  assert_ptr_equal(srcbuf.recv_trailer, dest->recv_trailer);
  assert_ptr_equal(srcbuf.end_trailers, dest->end_trailers);
  assert_ptr_equal(srcbuf.stop_sending, dest->stop_sending);
  assert_ptr_equal(srcbuf.end_stream, dest->end_stream);
  assert_ptr_equal(srcbuf.reset_stream, dest->reset_stream);
  assert_ptr_equal(srcbuf.shutdown, dest->shutdown);
  assert_ptr_equal(srcbuf.recv_settings, dest->recv_settings);
  assert_null(dest->recv_origin);
  assert_null(dest->end_origin);
}

void test_nghttp3_callbacks_convert_to_old(void) {
  nghttp3_callbacks src, *dest, destbuf;
  size_t v1len;

  v1len = nghttp3_callbackslen_version(NGHTTP3_CALLBACKS_V1);

  dest = malloc(v1len);

  memset(&src, 0, sizeof(src));
  src.acked_stream_data = acked_stream_data;
  src.stream_close = stream_close;
  src.recv_data = recv_data;
  src.deferred_consume = deferred_consume;
  src.begin_headers = begin_headers;
  src.recv_header = recv_header;
  src.end_headers = end_headers;
  src.begin_trailers = begin_trailers;
  src.recv_trailer = recv_trailer;
  src.end_trailers = end_trailers;
  src.stop_sending = stop_sending;
  src.end_stream = end_stream;
  src.reset_stream = reset_stream;
  src.shutdown = shutdown;
  src.recv_settings = recv_settings;
  src.recv_origin = recv_origin;
  src.end_origin = end_origin;

  nghttp3_callbacks_convert_to_old(NGHTTP3_CALLBACKS_V1, dest, &src);

  memset(&destbuf, 0, sizeof(destbuf));
  memcpy(&destbuf, dest, v1len);

  free(dest);

  assert_ptr_equal(src.acked_stream_data, destbuf.acked_stream_data);
  assert_ptr_equal(src.stream_close, destbuf.stream_close);
  assert_ptr_equal(src.recv_data, destbuf.recv_data);
  assert_ptr_equal(src.deferred_consume, destbuf.deferred_consume);
  assert_ptr_equal(src.begin_headers, destbuf.begin_headers);
  assert_ptr_equal(src.recv_header, destbuf.recv_header);
  assert_ptr_equal(src.end_headers, destbuf.end_headers);
  assert_ptr_equal(src.begin_trailers, destbuf.begin_trailers);
  assert_ptr_equal(src.recv_trailer, destbuf.recv_trailer);
  assert_ptr_equal(src.end_trailers, destbuf.end_trailers);
  assert_ptr_equal(src.stop_sending, destbuf.stop_sending);
  assert_ptr_equal(src.end_stream, destbuf.end_stream);
  assert_ptr_equal(src.reset_stream, destbuf.reset_stream);
  assert_ptr_equal(src.shutdown, destbuf.shutdown);
  assert_ptr_equal(src.recv_settings, destbuf.recv_settings);
  assert_null(destbuf.recv_origin);
  assert_null(destbuf.end_origin);
}
