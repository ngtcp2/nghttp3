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
#ifndef NGHTTP3_CONN_H
#define NGHTTP3_CONN_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <nghttp3/nghttp3.h>

#include "nghttp3_stream.h"
#include "nghttp3_map.h"
#include "nghttp3_qpack.h"
#include "nghttp3_tnode.h"
#include "nghttp3_idtr.h"

#define NGHTTP3_VARINT_MAX ((1ull << 62) - 1)

/* NGHTTP3_QPACK_ENCODER_MAX_TABLE_CAPACITY is the maximum dynamic
   table size for QPACK encoder. */
#define NGHTTP3_QPACK_ENCODER_MAX_TABLE_CAPACITY 16384

typedef struct {
  nghttp3_map_entry me;
  nghttp3_tnode node;
} nghttp3_placeholder;

typedef enum {
  NGHTTP3_CONN_FLAG_NONE = 0x0000,
  NGHTTP3_CONN_FLAG_SETTINGS_RECVED = 0x0001,
  NGHTTP3_CONN_FLAG_CONTROL_OPENED = 0x0002,
  NGHTTP3_CONN_FLAG_QPACK_ENCODER_OPENED = 0x0004,
  NGHTTP3_CONN_FLAG_QPACK_DECODER_OPENED = 0x0008,
} nghttp3_conn_flag;

struct nghttp3_conn {
  nghttp3_tnode root;
  nghttp3_conn_callbacks callbacks;
  nghttp3_map streams;
  nghttp3_map placeholders;
  nghttp3_qpack_decoder qdec;
  nghttp3_qpack_encoder qenc;
  nghttp3_pq qpack_blocked_streams;
  const nghttp3_mem *mem;
  void *user_data;
  int server;
  uint16_t flags;
  uint64_t next_seq;

  struct {
    nghttp3_conn_settings settings;
  } local;

  struct {
    struct {
      nghttp3_idtr idtr;
      /* max_client_streams is the cumulative number of client
         initiated bidirectional stream ID the remote endpoint can
         issue.  This field is used on server side only. */
      uint64_t max_client_streams;
    } bidi;
    nghttp3_conn_settings settings;
  } remote;

  struct {
    nghttp3_stream *ctrl;
    nghttp3_stream *qenc;
    nghttp3_stream *qdec;
  } tx;
};

nghttp3_stream *nghttp3_conn_find_stream(nghttp3_conn *conn, int64_t stream_id);

nghttp3_placeholder *nghttp3_conn_find_placeholder(nghttp3_conn *conn,
                                                   int64_t ph_id);

int nghttp3_conn_create_stream(nghttp3_conn *conn, nghttp3_stream **pstream,
                               int64_t stream_id);

int nghttp3_conn_create_stream_dependency(nghttp3_conn *conn,
                                          nghttp3_stream **pstream,
                                          int64_t stream_id, uint32_t weight,
                                          nghttp3_tnode *parent);

int nghttp3_conn_create_placeholder(nghttp3_conn *conn,
                                    nghttp3_placeholder **pph, int64_t ph_id,
                                    uint32_t weight, nghttp3_tnode *parent);

ssize_t nghttp3_conn_read_bidi(nghttp3_conn *conn, nghttp3_stream *stream,
                               const uint8_t *src, size_t srclen, int fin);

ssize_t nghttp3_conn_read_uni(nghttp3_conn *conn, nghttp3_stream *stream,
                              const uint8_t *src, size_t srclen, int fin);

ssize_t nghttp3_conn_read_control(nghttp3_conn *conn, nghttp3_stream *stream,
                                  const uint8_t *src, size_t srclen);

ssize_t nghttp3_conn_read_push(nghttp3_conn *conn, nghttp3_stream *stream,
                               const uint8_t *src, size_t srclen, int fin);

ssize_t nghttp3_conn_read_qpack_encoder(nghttp3_conn *conn, const uint8_t *src,
                                        size_t srclen);

ssize_t nghttp3_conn_read_qpack_decoder(nghttp3_conn *conn, const uint8_t *src,
                                        size_t srclen);

int nghttp3_conn_on_request_priority(nghttp3_conn *conn, nghttp3_stream *stream,
                                     const nghttp3_frame_priority *fr);

int nghttp3_conn_on_control_priority(nghttp3_conn *conn,
                                     const nghttp3_frame_priority *fr);

int nghttp3_conn_on_data(nghttp3_conn *conn, nghttp3_stream *stream,
                         const uint8_t *data, size_t datalen);

ssize_t nghttp3_conn_on_headers(nghttp3_conn *conn, nghttp3_stream *stream,
                                const uint8_t *data, size_t datalen, int fin);

int nghttp3_conn_on_settings_entry_received(nghttp3_conn *conn,
                                            const nghttp3_frame_settings *fr);

int nghttp3_conn_qpack_blocked_streams_push(nghttp3_conn *conn,
                                            nghttp3_stream *stream);

void nghttp3_conn_qpack_blocked_streams_pop(nghttp3_conn *conn);

/*
 * nghttp3_conn_get_next_tx_stream returns next stream to send.  It
 * returns NULL if there is no such stream.
 */
nghttp3_stream *nghttp3_conn_get_next_tx_stream(nghttp3_conn *conn);

int nghttp3_placeholder_new(nghttp3_placeholder **pph, int64_t ph_id,
                            uint64_t seq, uint32_t weight,
                            nghttp3_tnode *parent, const nghttp3_mem *mem);

void nghttp3_placeholder_del(nghttp3_placeholder *ph, const nghttp3_mem *mem);

#endif /* NGHTTP3_CONN_H */
