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
#include "nghttp3_conn.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "nghttp3_mem.h"
#include "nghttp3_macro.h"
#include "nghttp3_err.h"

static int stream_get_headers_type(nghttp3_stream *stream) {
  switch (stream->rx.hstate) {
  case NGHTTP3_HTTP_STATE_REQ_HEADERS_BEGIN:
  case NGHTTP3_HTTP_STATE_RESP_HEADERS_BEGIN:
    return NGHTTP3_HEADERS_TYPE_HEADER;
  case NGHTTP3_HTTP_STATE_REQ_TRAILERS_BEGIN:
  case NGHTTP3_HTTP_STATE_RESP_TRAILERS_BEGIN:
    return NGHTTP3_HEADERS_TYPE_TRAILER;
  case NGHTTP3_HTTP_STATE_RESP_PUSH_PROMISE_BEGIN:
    return NGHTTP3_HEADERS_TYPE_PUSH_PROMISE;
  default:
    /* Unreachable */
    assert(0);
  }
}

static int conn_call_begin_headers(nghttp3_conn *conn, nghttp3_stream *stream) {
  int rv;

  if (!conn->callbacks.begin_headers) {
    return 0;
  }

  rv = conn->callbacks.begin_headers(conn, stream->stream_id,
                                     stream_get_headers_type(stream),
                                     stream->user_data, conn->user_data);
  if (rv != 0) {
    /* TODO Allow ignore headers */
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

static int conn_call_end_headers(nghttp3_conn *conn, nghttp3_stream *stream) {
  int rv;

  if (!conn->callbacks.end_headers) {
    return 0;
  }

  rv = conn->callbacks.end_headers(conn, stream->stream_id, stream->user_data,
                                   conn->user_data);
  if (rv != 0) {
    /* TODO Allow ignore headers */
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

static int ricnt_less(const nghttp3_pq_entry *lhsx,
                      const nghttp3_pq_entry *rhsx) {
  nghttp3_stream *lhs =
      nghttp3_struct_of(lhsx, nghttp3_stream, qpack_blocked_pe);
  nghttp3_stream *rhs =
      nghttp3_struct_of(rhsx, nghttp3_stream, qpack_blocked_pe);

  return lhs->qpack_sctx.ricnt < rhs->qpack_sctx.ricnt;
}

static int conn_new(nghttp3_conn **pconn,
                    const nghttp3_conn_callbacks *callbacks,
                    const nghttp3_conn_settings *settings,
                    const nghttp3_mem *mem, void *user_data) {
  int rv;
  nghttp3_conn *conn;
  nghttp3_node_id nid;

  conn = nghttp3_mem_calloc(mem, 1, sizeof(nghttp3_conn));
  if (conn == NULL) {
    return NGHTTP3_ERR_NOMEM;
  }

  nghttp3_tnode_init(&conn->root,
                     nghttp3_node_id_init(&nid, NGHTTP3_NODE_ID_TYPE_ROOT, 0),
                     0, NGHTTP3_DEFAULT_WEIGHT, NULL, mem);

  rv = nghttp3_map_init(&conn->streams, mem);
  if (rv != 0) {
    goto streams_init_fail;
  }

  rv = nghttp3_map_init(&conn->placeholders, mem);
  if (rv != 0) {
    goto placeholders_init_fail;
  }

  rv = nghttp3_qpack_decoder_init(&conn->qdec,
                                  settings->qpack_max_table_capacity,
                                  settings->qpack_blocked_streams, mem);
  if (rv != 0) {
    goto qdec_init_fail;
  }

  rv = nghttp3_qpack_encoder_init(&conn->qenc, 0, 0, mem);
  if (rv != 0) {
    goto qenc_init_fail;
  }

  nghttp3_pq_init(&conn->qpack_blocked_streams, ricnt_less, mem);

  conn->callbacks = *callbacks;
  conn->local.settings = *settings;
  nghttp3_conn_settings_default(&conn->remote.settings);
  conn->mem = mem;
  conn->user_data = user_data;
  conn->next_seq = 0;

  *pconn = conn;

  return 0;

qenc_init_fail:
  nghttp3_qpack_decoder_free(&conn->qdec);
qdec_init_fail:
  nghttp3_map_free(&conn->placeholders);
placeholders_init_fail:
  nghttp3_map_free(&conn->streams);
streams_init_fail:
  nghttp3_mem_free(mem, conn);

  return rv;
}

int nghttp3_conn_client_new(nghttp3_conn **pconn,
                            const nghttp3_conn_callbacks *callbacks,
                            const nghttp3_conn_settings *settings,
                            const nghttp3_mem *mem, void *user_data) {
  return conn_new(pconn, callbacks, settings, mem, user_data);
}

int nghttp3_conn_server_new(nghttp3_conn **pconn,
                            const nghttp3_conn_callbacks *callbacks,
                            const nghttp3_conn_settings *settings,
                            const nghttp3_mem *mem, void *user_data) {
  int rv;

  rv = conn_new(pconn, callbacks, settings, mem, user_data);
  if (rv != 0) {
    return rv;
  }

  (*pconn)->server = 1;

  return 0;
}

static int free_placeholder(nghttp3_map_entry *ent, void *ptr) {
  nghttp3_placeholder *ph = nghttp3_struct_of(ent, nghttp3_placeholder, me);
  const nghttp3_mem *mem = ptr;

  nghttp3_placeholder_del(ph, mem);

  return 0;
}

static int free_stream(nghttp3_map_entry *ent, void *ptr) {
  nghttp3_stream *stream = nghttp3_struct_of(ent, nghttp3_stream, me);

  (void)ptr;

  nghttp3_stream_del(stream);

  return 0;
}

void nghttp3_conn_del(nghttp3_conn *conn) {
  if (conn == NULL) {
    return;
  }

  nghttp3_pq_free(&conn->qpack_blocked_streams);

  nghttp3_qpack_encoder_free(&conn->qenc);
  nghttp3_qpack_decoder_free(&conn->qdec);

  nghttp3_map_each_free(&conn->placeholders, free_placeholder,
                        (void *)conn->mem);
  nghttp3_map_free(&conn->placeholders);

  nghttp3_map_each_free(&conn->streams, free_stream, NULL);
  nghttp3_map_free(&conn->streams);

  nghttp3_tnode_free(&conn->root);

  nghttp3_mem_free(conn->mem, conn);
}

ssize_t nghttp3_conn_read_stream(nghttp3_conn *conn, int64_t stream_id,
                                 const uint8_t *src, size_t srclen, int fin) {
  nghttp3_stream *stream;
  int rv;

  stream = nghttp3_conn_find_stream(conn, stream_id);
  if (stream == NULL) {
    /* QUIC transport ensures that this is new stream. */
    rv = nghttp3_conn_create_stream(conn, &stream, stream_id);
    if (rv != 0) {
      return rv;
    }
    if (conn->server) {
      stream->rx.hstate = NGHTTP3_HTTP_STATE_REQ_INITIAL;
      stream->tx.hstate = NGHTTP3_HTTP_STATE_REQ_INITIAL;
    } else if (nghttp3_stream_uni(stream_id)) {
      stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;
      stream->tx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;
    } else {
      /* client doesn't expect to receive new bidirectional stream
         from server. */
      return NGHTTP3_ERR_HTTP_GENERAL_PROTOCOL_ERROR;
    }
  }

  if (srclen == 0 && !fin) {
    return 0;
  }

  if (nghttp3_stream_uni(stream_id)) {
    return nghttp3_conn_read_uni(conn, stream, src, srclen, fin);
  }
  return nghttp3_conn_read_bidi(conn, stream, src, srclen, fin);
}

static ssize_t conn_read_type(nghttp3_conn *conn, nghttp3_stream *stream,
                              const uint8_t *src, size_t srclen, int fin) {
  nghttp3_stream_read_state *rstate = &stream->rstate;
  nghttp3_varint_read_state *rvint = &rstate->rvint;
  ssize_t nread;
  int64_t stream_type;

  assert(srclen);

  nread = nghttp3_read_varint(rvint, src, srclen, fin);
  if (nread < 0) {
    return NGHTTP3_ERR_HTTP_GENERAL_PROTOCOL_ERROR;
  }

  if (rvint->left) {
    return nread;
  }

  stream_type = rvint->acc;
  nghttp3_varint_read_state_reset(rvint);

  switch (stream_type) {
  case NGHTTP3_STREAM_TYPE_CONTROL:
    if (conn->flags & NGHTTP3_CONN_FLAG_CONTROL_OPENED) {
      return NGHTTP3_ERR_HTTP_WRONG_STREAM_COUNT;
    }
    conn->flags |= NGHTTP3_CONN_FLAG_CONTROL_OPENED;
    stream->type = NGHTTP3_STREAM_TYPE_CONTROL;
    rstate->state = NGHTTP3_CTRL_STREAM_STATE_FRAME_TYPE;
    break;
  case NGHTTP3_STREAM_TYPE_PUSH:
    stream->type = NGHTTP3_STREAM_TYPE_PUSH;
    rstate->state = NGHTTP3_PUSH_STREAM_STATE_PUSH_ID;
    break;
  case NGHTTP3_STREAM_TYPE_QPACK_ENCODER:
    if (conn->flags & NGHTTP3_CONN_FLAG_QPACK_ENCODER_OPENED) {
      return NGHTTP3_ERR_HTTP_WRONG_STREAM_COUNT;
    }
    conn->flags |= NGHTTP3_CONN_FLAG_QPACK_ENCODER_OPENED;
    stream->type = NGHTTP3_STREAM_TYPE_QPACK_ENCODER;
    break;
  case NGHTTP3_STREAM_TYPE_QPACK_DECODER:
    if (conn->flags & NGHTTP3_CONN_FLAG_QPACK_DECODER_OPENED) {
      return NGHTTP3_ERR_HTTP_WRONG_STREAM_COUNT;
    }
    conn->flags |= NGHTTP3_CONN_FLAG_QPACK_DECODER_OPENED;
    stream->type = NGHTTP3_STREAM_TYPE_QPACK_DECODER;
    break;
  default:
    /* TODO Handle reserved stream type */
    return NGHTTP3_ERR_HTTP_UNKNOWN_STREAM_TYPE;
  }

  stream->flags |= NGHTTP3_STREAM_FLAG_TYPE_IDENTIFIED;

  return nread;
}

ssize_t nghttp3_conn_read_uni(nghttp3_conn *conn, nghttp3_stream *stream,
                              const uint8_t *src, size_t srclen, int fin) {
  ssize_t nread = 0;
  ssize_t nconsumed = 0;

  assert(srclen);

  if (!(stream->flags & NGHTTP3_STREAM_FLAG_TYPE_IDENTIFIED)) {
    nread = conn_read_type(conn, stream, src, srclen, fin);
    if (nread < 0) {
      return (int)nread;
    }
    if (!(stream->flags & NGHTTP3_STREAM_FLAG_TYPE_IDENTIFIED)) {
      assert((size_t)nread == srclen);
      return (ssize_t)srclen;
    }

    src += nread;
    srclen -= (size_t)nread;

    if (srclen == 0) {
      return nread;
    }
  }

  switch (stream->type) {
  case NGHTTP3_STREAM_TYPE_CONTROL:
    if (fin) {
      return NGHTTP3_ERR_HTTP_CLOSED_CRITICAL_STREAM;
    }
    nconsumed = nghttp3_conn_read_control(conn, stream, src, srclen);
    break;
  case NGHTTP3_STREAM_TYPE_PUSH:
    nconsumed = nghttp3_conn_read_push(conn, stream, src, srclen, fin);
    break;
  case NGHTTP3_STREAM_TYPE_QPACK_ENCODER:
    if (fin) {
      return NGHTTP3_ERR_HTTP_CLOSED_CRITICAL_STREAM;
    }
    nconsumed = nghttp3_conn_read_qpack_encoder(conn, src, srclen);
    break;
  case NGHTTP3_STREAM_TYPE_QPACK_DECODER:
    if (fin) {
      return NGHTTP3_ERR_HTTP_CLOSED_CRITICAL_STREAM;
    }
    nconsumed = nghttp3_conn_read_qpack_decoder(conn, src, srclen);
    break;
  default:
    /* unreachable */
    assert(0);
  }

  if (nconsumed < 0) {
    return nconsumed;
  }

  return nread + nconsumed;
}

static int frame_fin(nghttp3_stream_read_state *rstate, size_t len) {
  return (int64_t)len >= rstate->left;
}

ssize_t nghttp3_conn_read_control(nghttp3_conn *conn, nghttp3_stream *stream,
                                  const uint8_t *src, size_t srclen) {
  const uint8_t *p = src, *end = src + srclen;
  int rv;
  nghttp3_stream_read_state *rstate = &stream->rstate;
  nghttp3_varint_read_state *rvint = &rstate->rvint;
  ssize_t nread;
  size_t nconsumed = 0;
  int busy = 0;
  size_t len;

  assert(srclen);

  for (; p != end || busy;) {
    busy = 0;
    switch (rstate->state) {
    case NGHTTP3_CTRL_STREAM_STATE_FRAME_TYPE:
      assert(end - p > 0);
      nread = nghttp3_read_varint(rvint, p, (size_t)(end - p), /* fin = */ 0);
      if (nread < 0) {
        return NGHTTP3_ERR_HTTP_GENERAL_PROTOCOL_ERROR;
      }

      p += nread;
      nconsumed += (size_t)nread;
      if (rvint->left) {
        return (ssize_t)nconsumed;
      }

      rstate->fr.hd.type = rvint->acc;
      nghttp3_varint_read_state_reset(rvint);
      rstate->state = NGHTTP3_CTRL_STREAM_STATE_FRAME_LENGTH;
      if (p == end) {
        break;
      }
      /* Fall through */
    case NGHTTP3_CTRL_STREAM_STATE_FRAME_LENGTH:
      assert(end - p > 0);
      nread = nghttp3_read_varint(rvint, p, (size_t)(end - p), /* fin = */ 0);
      if (nread < 0) {
        return nghttp3_err_malformed_frame(rstate->fr.hd.type);
      }

      p += nread;
      nconsumed += (size_t)nread;
      if (rvint->left) {
        return (ssize_t)nconsumed;
      }

      rstate->left = rstate->fr.hd.length = rvint->acc;
      nghttp3_varint_read_state_reset(rvint);

      if (!(conn->flags & NGHTTP3_CONN_FLAG_SETTINGS_RECVED)) {
        if (rstate->fr.hd.type != NGHTTP3_FRAME_SETTINGS) {
          return NGHTTP3_ERR_HTTP_MISSING_SETTINGS;
        }
        conn->flags |= NGHTTP3_CONN_FLAG_SETTINGS_RECVED;
      } else if (rstate->fr.hd.type == NGHTTP3_FRAME_SETTINGS) {
        return NGHTTP3_ERR_HTTP_UNEXPECTED_FRAME;
      }

      switch (rstate->fr.hd.type) {
      case NGHTTP3_FRAME_PRIORITY:
        if (rstate->left == 0) {
          return nghttp3_err_malformed_frame(NGHTTP3_FRAME_PRIORITY);
        }
        rstate->state = NGHTTP3_CTRL_STREAM_STATE_PRIORITY;
        break;
      case NGHTTP3_FRAME_CANCEL_PUSH:
        if (rstate->left == 0) {
          return nghttp3_err_malformed_frame(NGHTTP3_FRAME_CANCEL_PUSH);
        }
        rstate->state = NGHTTP3_CTRL_STREAM_STATE_CANCEL_PUSH;
        break;
      case NGHTTP3_FRAME_SETTINGS:
        /* SETTINGS frame might be empty. */
        if (rstate->left == 0) {
          nghttp3_stream_read_state_reset(rstate);
          break;
        }
        rstate->state = NGHTTP3_CTRL_STREAM_STATE_SETTINGS;
        break;
      case NGHTTP3_FRAME_GOAWAY:
        if (rstate->left == 0) {
          return nghttp3_err_malformed_frame(NGHTTP3_FRAME_GOAWAY);
        }
        rstate->state = NGHTTP3_CTRL_STREAM_STATE_GOAWAY;
        break;
      case NGHTTP3_FRAME_MAX_PUSH_ID:
        if (rstate->left == 0) {
          return nghttp3_err_malformed_frame(NGHTTP3_FRAME_MAX_PUSH_ID);
        }
        rstate->state = NGHTTP3_CTRL_STREAM_STATE_MAX_PUSH_ID;
        break;
      case NGHTTP3_FRAME_DATA:
      case NGHTTP3_FRAME_HEADERS:
      case NGHTTP3_FRAME_PUSH_PROMISE:
      case NGHTTP3_FRAME_DUPLICATE_PUSH:
        return NGHTTP3_ERR_HTTP_WRONG_STREAM;
      default:
        /* TODO Handle reserved frame type */
        busy = 1;
        rstate->state = NGHTTP3_CTRL_STREAM_STATE_IGN_FRAME;
        break;
      }
      break;
    case NGHTTP3_CTRL_STREAM_STATE_PRIORITY:
      /* TODO Not implemented yet */
      rstate->state = NGHTTP3_CTRL_STREAM_STATE_IGN_FRAME;
      break;
    case NGHTTP3_CTRL_STREAM_STATE_CANCEL_PUSH:
      /* TODO Not implemented yet */
      rstate->state = NGHTTP3_CTRL_STREAM_STATE_IGN_FRAME;
      break;
    case NGHTTP3_CTRL_STREAM_STATE_SETTINGS:
      for (; p != end;) {
        if (rstate->left == 0) {
          nghttp3_stream_read_state_reset(rstate);
          break;
        }
        /* Read Identifier */
        len = (size_t)nghttp3_min(rstate->left, (int64_t)(end - p));
        assert(len > 0);
        nread = nghttp3_read_varint(rvint, p, len, frame_fin(rstate, len));
        if (nread < 0) {
          return nghttp3_err_malformed_frame(NGHTTP3_FRAME_SETTINGS);
        }

        p += nread;
        nconsumed += (size_t)nread;
        rstate->left -= nread;
        if (rvint->left) {
          rstate->state = NGHTTP3_CTRL_STREAM_STATE_SETTINGS_ID;
          return (ssize_t)nconsumed;
        }
        rstate->fr.settings.iv[0].id = rvint->acc;
        nghttp3_varint_read_state_reset(rvint);

        /* Read Value */
        if (rstate->left == 0) {
          return nghttp3_err_malformed_frame(NGHTTP3_FRAME_SETTINGS);
        }

        len -= (size_t)nread;
        if (len == 0) {
          rstate->state = NGHTTP3_CTRL_STREAM_STATE_SETTINGS_VALUE;
          break;
        }

        nread = nghttp3_read_varint(rvint, p, len, frame_fin(rstate, len));
        if (nread < 0) {
          return nghttp3_err_malformed_frame(NGHTTP3_FRAME_SETTINGS);
        }

        p += nread;
        nconsumed += (size_t)nread;
        rstate->left -= nread;
        if (rvint->left) {
          rstate->state = NGHTTP3_CTRL_STREAM_STATE_SETTINGS_VALUE;
          return (ssize_t)nconsumed;
        }
        rstate->fr.settings.iv[0].value = rvint->acc;
        nghttp3_varint_read_state_reset(rvint);

        rv =
            nghttp3_conn_on_settings_entry_received(conn, &rstate->fr.settings);
        if (rv != 0) {
          return rv;
        }
      }
      break;
    case NGHTTP3_CTRL_STREAM_STATE_SETTINGS_ID:
      len = (size_t)nghttp3_min(rstate->left, (int64_t)(end - p));
      assert(len > 0);
      nread = nghttp3_read_varint(rvint, p, len, frame_fin(rstate, len));
      if (nread < 0) {
        return nghttp3_err_malformed_frame(NGHTTP3_FRAME_SETTINGS);
      }

      p += nread;
      nconsumed += (size_t)nread;
      rstate->left -= nread;
      if (rvint->left) {
        return (ssize_t)nconsumed;
      }
      rstate->fr.settings.iv[0].id = rvint->acc;
      nghttp3_varint_read_state_reset(rvint);

      if (rstate->left == 0) {
        return nghttp3_err_malformed_frame(NGHTTP3_FRAME_SETTINGS);
      }

      rstate->state = NGHTTP3_CTRL_STREAM_STATE_SETTINGS_VALUE;
      /* Fall through */
    case NGHTTP3_CTRL_STREAM_STATE_SETTINGS_VALUE:
      len = (size_t)nghttp3_min(rstate->left, (int64_t)(end - p));
      assert(len > 0);
      nread = nghttp3_read_varint(rvint, p, len, frame_fin(rstate, len));
      if (nread < 0) {
        return nghttp3_err_malformed_frame(NGHTTP3_FRAME_SETTINGS);
      }

      p += nread;
      nconsumed += (size_t)nread;
      rstate->left -= nread;
      if (rvint->left) {
        return (ssize_t)nconsumed;
      }
      rstate->fr.settings.iv[0].value = rvint->acc;
      nghttp3_varint_read_state_reset(rvint);

      rv = nghttp3_conn_on_settings_entry_received(conn, &rstate->fr.settings);
      if (rv != 0) {
        return rv;
      }

      if (rstate->left) {
        rstate->state = NGHTTP3_CTRL_STREAM_STATE_SETTINGS;
        break;
      }

      nghttp3_stream_read_state_reset(rstate);
      break;
    case NGHTTP3_CTRL_STREAM_STATE_GOAWAY:
      /* TODO Not implemented yet */
      rstate->state = NGHTTP3_CTRL_STREAM_STATE_IGN_FRAME;
      break;
    case NGHTTP3_CTRL_STREAM_STATE_MAX_PUSH_ID:
      /* TODO Not implemented yet */
      rstate->state = NGHTTP3_CTRL_STREAM_STATE_IGN_FRAME;
      break;
    case NGHTTP3_CTRL_STREAM_STATE_IGN_FRAME:
      len = (size_t)nghttp3_min(rstate->left, (int64_t)(end - p));
      p += len;
      nconsumed += len;
      rstate->left -= (int64_t)len;

      if (rstate->left) {
        return (ssize_t)nconsumed;
      }

      nghttp3_stream_read_state_reset(rstate);
      break;
    default:
      /* unreachable */
      assert(0);
    }
  }

  return (ssize_t)nconsumed;
}

ssize_t nghttp3_conn_read_push(nghttp3_conn *conn, nghttp3_stream *stream,
                               const uint8_t *src, size_t srclen, int fin) {
  (void)conn;
  (void)stream;
  (void)src;
  (void)srclen;
  (void)fin;
  return 0;
}

ssize_t nghttp3_conn_read_qpack_encoder(nghttp3_conn *conn, const uint8_t *src,
                                        size_t srclen) {
  ssize_t nread = nghttp3_qpack_decoder_read_encoder(&conn->qdec, src, srclen);
  nghttp3_stream *stream;
  nghttp3_buf *buf;
  int rv;

  if (nread < 0) {
    return nread;
  }

  for (; !nghttp3_pq_empty(&conn->qpack_blocked_streams);) {
    stream = nghttp3_struct_of(nghttp3_pq_top(&conn->qpack_blocked_streams),
                               nghttp3_stream, qpack_blocked_pe);
    if (nghttp3_qpack_stream_context_get_ricnt(&stream->qpack_sctx) >
        nghttp3_qpack_decoder_get_icnt(&conn->qdec)) {
      break;
    }

    nghttp3_conn_qpack_blocked_streams_pop(conn);

    stream->flags &= (uint16_t)~NGHTTP3_STREAM_FLAG_QPACK_DECODE_BLOCKED;

    for (; nghttp3_ringbuf_len(&stream->inq);) {
      buf = nghttp3_ringbuf_get(&stream->inq, 0);

      nread =
          nghttp3_conn_read_bidi(conn, stream, buf->pos, nghttp3_buf_len(buf),
                                 stream->flags & NGHTTP3_STREAM_FLAG_READ_EOF);
      if (nread < 0) {
        return nread;
      }

      buf->pos += nread;

      if (conn->callbacks.deferred_consume) {
        rv = conn->callbacks.deferred_consume(conn, stream->stream_id,
                                              (size_t)nread, stream->user_data,
                                              conn->user_data);
        if (rv != 0) {
          return NGHTTP3_ERR_CALLBACK_FAILURE;
        }
      }

      if (nghttp3_buf_len(buf) == 0) {
        nghttp3_buf_free(buf, stream->mem);
        nghttp3_ringbuf_pop_front(&stream->inq);
      }

      if (stream->flags & NGHTTP3_STREAM_FLAG_QPACK_DECODE_BLOCKED) {
        break;
      }
    }
  }

  return nread;
}

ssize_t nghttp3_conn_read_qpack_decoder(nghttp3_conn *conn, const uint8_t *src,
                                        size_t srclen) {
  return nghttp3_qpack_encoder_read_decoder(&conn->qenc, src, srclen);
}

ssize_t nghttp3_conn_read_bidi(nghttp3_conn *conn, nghttp3_stream *stream,
                               const uint8_t *src, size_t srclen, int fin) {
  const uint8_t *p = src, *end = src + srclen;
  int rv;
  nghttp3_stream_read_state *rstate = &stream->rstate;
  nghttp3_varint_read_state *rvint = &rstate->rvint;
  ssize_t nread;
  size_t nconsumed = 0;
  int busy = 0;
  size_t len;

  if (fin) {
    assert(!(stream->flags & NGHTTP3_STREAM_FLAG_READ_EOF));
    stream->flags |= NGHTTP3_STREAM_FLAG_READ_EOF;
  }

  if (stream->flags & NGHTTP3_STREAM_FLAG_QPACK_DECODE_BLOCKED) {
    if (srclen == 0) {
      return 0;
    }

    rv = nghttp3_stream_buffer_data(stream, p, (size_t)(end - p));
    if (rv != 0) {
      return rv;
    }
    return 0;
  }

  for (; p != end || busy;) {
    busy = 0;
    switch (rstate->state) {
    case NGHTTP3_REQ_STREAM_STATE_FRAME_TYPE:
      assert(end - p > 0);
      nread = nghttp3_read_varint(rvint, p, (size_t)(end - p), fin);
      if (nread < 0) {
        return NGHTTP3_ERR_HTTP_GENERAL_PROTOCOL_ERROR;
      }

      p += nread;
      nconsumed += (size_t)nread;
      if (rvint->left) {
        goto almost_done;
      }

      rstate->fr.hd.type = rvint->acc;
      nghttp3_varint_read_state_reset(rvint);
      rstate->state = NGHTTP3_REQ_STREAM_STATE_FRAME_LENGTH;
      if (p == end) {
        break;
      }
      /* Fall through */
    case NGHTTP3_REQ_STREAM_STATE_FRAME_LENGTH:
      assert(end - p > 0);
      nread = nghttp3_read_varint(rvint, p, (size_t)(end - p), fin);
      if (nread < 0) {
        return nghttp3_err_malformed_frame(rstate->fr.hd.type);
      }

      p += nread;
      nconsumed += (size_t)nread;
      if (rvint->left) {
        goto almost_done;
      }

      rstate->left = rstate->fr.hd.length = rvint->acc;
      nghttp3_varint_read_state_reset(rvint);

      /* TODO Verify that PRIORITY is only allowed at the beginning of
         request stream */
      switch (rstate->fr.hd.type) {
      case NGHTTP3_FRAME_DATA:
        rv = nghttp3_stream_transit_rx_http_state(
            stream, NGHTTP3_HTTP_EVENT_DATA_BEGIN);
        if (rv != 0) {
          return rv;
        }
        /* DATA frame might be empty. */
        if (rstate->left == 0) {
          nghttp3_stream_read_state_reset(rstate);
          break;
        }
        rstate->state = NGHTTP3_REQ_STREAM_STATE_DATA;
        break;
      case NGHTTP3_FRAME_HEADERS:
        rv = nghttp3_stream_transit_rx_http_state(
            stream, NGHTTP3_HTTP_EVENT_HEADERS_BEGIN);
        if (rv != 0) {
          return rv;
        }
        if (rstate->left == 0) {
          rv = nghttp3_stream_empty_headers_allowed(stream);
          if (rv != 0) {
            return rv;
          }
          busy = 1;
        }

        rv = conn_call_begin_headers(conn, stream);
        if (rv != 0) {
          return rv;
        }

        rstate->state = NGHTTP3_REQ_STREAM_STATE_HEADERS;
        break;
      case NGHTTP3_FRAME_PUSH_PROMISE:
        if (rstate->left == 0) {
          return nghttp3_err_malformed_frame(NGHTTP3_FRAME_PUSH_PROMISE);
        }
        rstate->state = NGHTTP3_REQ_STREAM_STATE_PUSH_PROMISE;
        break;
      case NGHTTP3_FRAME_DUPLICATE_PUSH:
        if (rstate->left == 0) {
          return nghttp3_err_malformed_frame(NGHTTP3_FRAME_DUPLICATE_PUSH);
        }
        rstate->state = NGHTTP3_REQ_STREAM_STATE_DUPLICATE_PUSH;
        break;
      case NGHTTP3_FRAME_PRIORITY:
        if (rstate->left < 2) {
          return nghttp3_err_malformed_frame(NGHTTP3_FRAME_PRIORITY);
        }
        rv = nghttp3_stream_transit_rx_http_state(
            stream, NGHTTP3_HTTP_EVENT_PRIORITY_BEGIN);
        if (rv != 0) {
          return rv;
        }
        rstate->state = NGHTTP3_REQ_STREAM_STATE_PRIORITY;
        break;
      case NGHTTP3_FRAME_CANCEL_PUSH:
      case NGHTTP3_FRAME_SETTINGS:
      case NGHTTP3_FRAME_GOAWAY:
      case NGHTTP3_FRAME_MAX_PUSH_ID:
        return NGHTTP3_ERR_HTTP_WRONG_STREAM;
      default:
        /* TODO Handle reserved frame type */
        busy = 1;
        rstate->state = NGHTTP3_REQ_STREAM_STATE_IGN_FRAME;
        break;
      }
      break;
    case NGHTTP3_REQ_STREAM_STATE_PRIORITY:
      if (nghttp3_frame_pri_elem_type(*p) != NGHTTP3_PRI_ELEM_TYPE_CURRENT) {
        return nghttp3_err_malformed_frame(NGHTTP3_FRAME_PRIORITY);
      }
      rstate->fr.priority.pt = NGHTTP3_PRI_ELEM_TYPE_CURRENT;
      rstate->fr.priority.dt = nghttp3_frame_elem_dep_type(*p);

      ++p;
      ++nconsumed;
      --rstate->left;

      if (rstate->fr.priority.dt == NGHTTP3_ELEM_DEP_TYPE_ROOT) {
        rstate->fr.priority.elem_dep_id = 0;
        rstate->state = NGHTTP3_REQ_STREAM_STATE_PRIORITY_WEIGHT;
        break;
      }

      rstate->state = NGHTTP3_REQ_STREAM_STATE_PRIORITY_ELEM_DEP_ID;
      if (p == end) {
        goto almost_done;
      }
      /* Fall through */
    case NGHTTP3_REQ_STREAM_STATE_PRIORITY_ELEM_DEP_ID:
      len = (size_t)nghttp3_min(rstate->left, (int64_t)(end - p));
      nread = nghttp3_read_varint(rvint, p, (size_t)(end - p),
                                  (int64_t)len == rstate->left);
      if (nread < 0) {
        return nghttp3_err_malformed_frame(NGHTTP3_FRAME_PRIORITY);
      }

      p += nread;
      nconsumed += (size_t)nread;
      rstate->left -= nread;
      if (rvint->left) {
        goto almost_done;
      }

      rstate->fr.priority.elem_dep_id = rvint->acc;
      nghttp3_varint_read_state_reset(rvint);

      if (rstate->left != 1) {
        return nghttp3_err_malformed_frame(NGHTTP3_FRAME_PRIORITY);
      }

      rstate->state = NGHTTP3_REQ_STREAM_STATE_PRIORITY_WEIGHT;
      if (p == end) {
        goto almost_done;
      }
      /* Fall through */
    case NGHTTP3_REQ_STREAM_STATE_PRIORITY_WEIGHT:
      assert(p != end);
      assert(rstate->left == 1);

      rstate->fr.priority.weight = (uint32_t)(*p + 1);

      ++p;
      ++nconsumed;

      rv = nghttp3_conn_on_request_priority(conn, stream, &rstate->fr.priority);
      if (rv != 0) {
        return rv;
      }

      rv = nghttp3_stream_transit_rx_http_state(
          stream, NGHTTP3_HTTP_EVENT_PRIORITY_END);

      assert(0 == rv);

      nghttp3_stream_read_state_reset(rstate);
      break;
    case NGHTTP3_REQ_STREAM_STATE_DATA:
      len = (size_t)nghttp3_min(rstate->left, (int64_t)(end - p));
      rv = nghttp3_conn_on_data(conn, stream, p, len);
      if (rv != 0) {
        return rv;
      }

      p += len;
      nconsumed += len;
      rstate->left -= (int64_t)len;

      if (rstate->left) {
        goto almost_done;
      }

      rv = nghttp3_stream_transit_rx_http_state(stream,
                                                NGHTTP3_HTTP_EVENT_DATA_END);
      assert(0 == rv);

      nghttp3_stream_read_state_reset(rstate);
      break;
    case NGHTTP3_REQ_STREAM_STATE_HEADERS:
      len = (size_t)nghttp3_min(rstate->left, (int64_t)(end - p));
      nread = nghttp3_conn_on_headers(conn, stream, p, len,
                                      (int64_t)len == rstate->left);
      if (nread < 0) {
        return nread;
      }

      p += nread;
      nconsumed += (size_t)nread;
      rstate->left -= nread;

      if (stream->flags & NGHTTP3_STREAM_FLAG_QPACK_DECODE_BLOCKED) {
        rv = nghttp3_stream_buffer_data(stream, p, (size_t)(end - p));
        if (rv != 0) {
          return rv;
        }
        return (ssize_t)nconsumed;
      }

      if (rstate->left) {
        goto almost_done;
      }

      rv = conn_call_end_headers(conn, stream);
      if (rv != 0) {
        return rv;
      }

      rv = nghttp3_stream_transit_rx_http_state(stream,
                                                NGHTTP3_HTTP_EVENT_HEADERS_END);
      assert(0 == rv);

      nghttp3_stream_read_state_reset(rstate);
      break;
    }
  }

almost_done:
  if ((stream->flags & NGHTTP3_STREAM_FLAG_READ_EOF)) {
    switch (rstate->state) {
    case NGHTTP3_REQ_STREAM_STATE_FRAME_TYPE:
      if (rvint->left) {
        return NGHTTP3_ERR_HTTP_GENERAL_PROTOCOL_ERROR;
      }
      return nghttp3_stream_transit_rx_http_state(stream,
                                                  NGHTTP3_HTTP_EVENT_MSG_END);
    default:
      return nghttp3_err_malformed_frame(rstate->fr.hd.type);
    }
  }

  return (ssize_t)nconsumed;
}

int nghttp3_conn_on_data(nghttp3_conn *conn, nghttp3_stream *stream,
                         const uint8_t *data, size_t datalen) {
  int rv;

  if (!conn->callbacks.recv_data) {
    return 0;
  }

  rv = conn->callbacks.recv_data(conn, stream->stream_id, data, datalen,
                                 stream->user_data, conn->user_data);
  if (rv != 0) {
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

static int conn_ensure_dependency(nghttp3_conn *conn,
                                  nghttp3_tnode **pdep_tnode,
                                  const nghttp3_node_id *dep_nid,
                                  nghttp3_tnode *tnode) {
  nghttp3_tnode *dep_tnode = NULL;
  nghttp3_stream *dep_stream;
  nghttp3_placeholder *dep_ph;
  int rv;

  switch (dep_nid->type) {
  case NGHTTP3_NODE_ID_TYPE_STREAM:
    if (!nghttp3_client_stream_bidi(dep_nid->id)) {
      return nghttp3_err_malformed_frame(NGHTTP3_FRAME_PRIORITY);
    }
    if (dep_nid->id > conn->rx.max_client_stream_id_bidi) {
      return NGHTTP3_ERR_HTTP_LIMIT_EXCEEDED;
    }

    dep_stream = nghttp3_conn_find_stream(conn, dep_nid->id);
    if (dep_stream == NULL) {
      rv = nghttp3_conn_create_stream(conn, &dep_stream, dep_nid->id);
      if (rv != 0) {
        return rv;
      }
    } else if (nghttp3_tnode_find_ascendant(&dep_stream->node, &tnode->nid) !=
               NULL) {
      nghttp3_tnode_remove(&dep_stream->node);
      nghttp3_tnode_insert(&dep_stream->node, tnode->parent);

      if (nghttp3_stream_require_schedule(dep_stream) ||
          nghttp3_tnode_has_active_descendant(&dep_stream->node)) {
        rv = nghttp3_stream_schedule(dep_stream);
        if (rv != 0) {
          return rv;
        }
      }
    }
    dep_tnode = &dep_stream->node;
    break;
  case NGHTTP3_NODE_ID_TYPE_PUSH:
    /* TODO Not implemented */
    break;
  case NGHTTP3_NODE_ID_TYPE_PLACEHOLDER:
    if ((uint64_t)dep_nid->id >= conn->local.settings.num_placeholders) {
      return NGHTTP3_ERR_HTTP_LIMIT_EXCEEDED;
    }

    dep_ph = nghttp3_conn_find_placeholder(conn, dep_nid->id);
    if (dep_ph == NULL) {
      rv = nghttp3_conn_create_placeholder(conn, &dep_ph, dep_nid->id,
                                           NGHTTP3_DEFAULT_WEIGHT, &conn->root);
      if (rv != 0) {
        return rv;
      }
    } else if (nghttp3_tnode_find_ascendant(&dep_ph->node, &tnode->nid) !=
               NULL) {
      nghttp3_tnode_remove(&dep_ph->node);
      nghttp3_tnode_insert(&dep_ph->node, tnode->parent);

      if (nghttp3_tnode_has_active_descendant(&dep_ph->node)) {
        rv = nghttp3_tnode_schedule(&dep_ph->node, 0);
        if (rv != 0) {
          return rv;
        }
      }
    }
    dep_tnode = &dep_ph->node;
    break;
  case NGHTTP3_NODE_ID_TYPE_ROOT:
    dep_tnode = &conn->root;
    break;
  }

  *pdep_tnode = dep_tnode;

  return 0;
}

int nghttp3_conn_on_request_priority(nghttp3_conn *conn, nghttp3_stream *stream,
                                     const nghttp3_frame_priority *fr) {
  nghttp3_node_id dep_nid;
  nghttp3_tnode *dep_tnode = NULL;
  int rv;

  nghttp3_node_id_init(&dep_nid, fr->dt, fr->elem_dep_id);

  if (nghttp3_node_id_eq(&stream->node.nid, &dep_nid)) {
    return nghttp3_err_malformed_frame(NGHTTP3_FRAME_PRIORITY);
  }

  if (stream->node.weight == fr->weight &&
      nghttp3_node_id_eq(&stream->node.parent->nid, &dep_nid)) {
    return 0;
  }

  if (dep_nid.type == NGHTTP3_NODE_ID_TYPE_STREAM &&
      dep_nid.id > stream->stream_id) {
    return nghttp3_err_malformed_frame(NGHTTP3_FRAME_PRIORITY);
  }

  if (dep_nid.type == NGHTTP3_NODE_ID_TYPE_PUSH) {
    /* TODO Not implemented */
    return 0;
  }

  rv = conn_ensure_dependency(conn, &dep_tnode, &dep_nid, &stream->node);
  if (rv != 0) {
    return rv;
  }

  assert(dep_tnode != NULL);

  nghttp3_tnode_remove(&stream->node);
  stream->node.weight = fr->weight;
  nghttp3_tnode_insert(&stream->node, dep_tnode);

  if (nghttp3_stream_require_schedule(stream)) {
    rv = nghttp3_stream_schedule(stream);
    if (rv != 0) {
      return rv;
    }
  }

  return 0;
}

static ssize_t conn_decode_headers(nghttp3_conn *conn, nghttp3_stream *stream,
                                   const uint8_t *src, size_t srclen, int fin) {
  ssize_t nread;
  int rv;
  nghttp3_qpack_decoder *qdec = &conn->qdec;
  nghttp3_qpack_nv nv;
  uint8_t flags;
  nghttp3_buf buf;

  assert(srclen);

  nghttp3_buf_wrap_init(&buf, (uint8_t *)src, srclen);
  buf.last = buf.end;

  for (;;) {
    nread = nghttp3_qpack_decoder_read_request(qdec, &stream->qpack_sctx, &nv,
                                               &flags, buf.pos,
                                               nghttp3_buf_len(&buf), fin);

    if (nread < 0) {
      return (int)nread;
    }

    buf.pos += nread;

    if (flags & NGHTTP3_QPACK_DECODE_FLAG_BLOCKED) {
      if (conn->local.settings.qpack_blocked_streams <=
          nghttp3_pq_size(&conn->qpack_blocked_streams)) {
        return NGHTTP3_ERR_QPACK_DECOMPRESSION_FAILED;
      }

      stream->flags |= NGHTTP3_STREAM_FLAG_QPACK_DECODE_BLOCKED;
      rv = nghttp3_conn_qpack_blocked_streams_push(conn, stream);
      if (rv != 0) {
        return rv;
      }
      break;
    }

    if (flags & NGHTTP3_QPACK_DECODE_FLAG_FINAL) {
      nghttp3_qpack_stream_context_reset(&stream->qpack_sctx);
      break;
    }

    if (nread == 0) {
      break;
    }

    if (flags & NGHTTP3_QPACK_DECODE_FLAG_EMIT) {
      if (conn->callbacks.recv_header) {
        rv = conn->callbacks.recv_header(conn, stream->stream_id, nv.name,
                                         nv.value, nv.flags, stream->user_data,
                                         conn->user_data);
      } else {
        rv = 0;
      }

      nghttp3_rcbuf_decref(nv.name);
      nghttp3_rcbuf_decref(nv.value);

      if (rv != 0) {
        return rv;
      }
    }
  }

  return buf.pos - src;
}

ssize_t nghttp3_conn_on_headers(nghttp3_conn *conn, nghttp3_stream *stream,
                                const uint8_t *src, size_t srclen, int fin) {
  if (srclen == 0 && !fin) {
    return 0;
  }

  return conn_decode_headers(conn, stream, src, srclen, fin);
}

int nghttp3_conn_on_settings_entry_received(nghttp3_conn *conn,
                                            const nghttp3_frame_settings *fr) {
  const nghttp3_settings_entry *ent = &fr->iv[0];
  nghttp3_conn_settings *dest = &conn->remote.settings;
  int rv;
  uint32_t max_table_capacity = NGHTTP3_QPACK_ENCODER_MAX_TABLE_CAPACITY;

  /* TODO Check for duplicates */
  switch (ent->id) {
  case NGHTTP3_SETTINGS_ID_MAX_HEADER_LIST_SIZE:
    dest->max_header_list_size = (uint64_t)ent->value;
    break;
  case NGHTTP3_SETTINGS_ID_NUM_PLACEHOLDERS:
    dest->num_placeholders = (uint64_t)ent->value;
    break;
  case NGHTTP3_SETTINGS_ID_QPACK_MAX_TABLE_CAPACITY:
    if (ent->value > NGHTTP3_QPACK_MAX_MAX_TABLE_CAPACITY) {
      /* TODO What is the best error code for this situation? */
      return NGHTTP3_ERR_HTTP_INTERNAL_ERROR;
    }
    dest->qpack_max_table_capacity = (uint32_t)ent->value;
    max_table_capacity =
        nghttp3_min(max_table_capacity, dest->qpack_max_table_capacity);
    rv = nghttp3_qpack_encoder_set_hard_max_dtable_size(&conn->qenc,
                                                        max_table_capacity);
    if (rv != 0) {
      return rv;
    }
    break;
  case NGHTTP3_SETTINGS_ID_QPACK_BLOCKED_STREAMS:
    if (ent->value > NGHTTP3_QPACK_MAX_BLOCKED_STREAMS) {
      /* TODO What is the best error code for this situation? */
      return NGHTTP3_ERR_HTTP_INTERNAL_ERROR;
    }
    dest->qpack_blocked_streams = (uint16_t)ent->value;
    rv = nghttp3_qpack_encoder_set_max_blocked(&conn->qenc,
                                               dest->qpack_blocked_streams);
    if (rv != 0) {
      return rv;
    }
    break;
  default:
    /* Ignore unknown settings ID */
    break;
  }

  return 0;
}

static int conn_stream_acked_data(nghttp3_stream *stream, int64_t stream_id,
                                  size_t datalen, void *user_data) {
  nghttp3_conn *conn = stream->conn;
  int rv;

  if (!conn->callbacks.acked_stream_data) {
    return 0;
  }

  rv = conn->callbacks.acked_stream_data(conn, stream_id, datalen, user_data,
                                         conn->user_data);
  if (rv != 0) {
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

int nghttp3_conn_create_stream(nghttp3_conn *conn, nghttp3_stream **pstream,
                               int64_t stream_id) {
  nghttp3_stream *stream;
  int rv;
  nghttp3_stream_callbacks callbacks = {
      conn_stream_acked_data,
  };

  rv = nghttp3_stream_new(&stream, stream_id, conn->next_seq,
                          NGHTTP3_DEFAULT_WEIGHT, &conn->root, &callbacks,
                          conn->mem);
  if (rv != 0) {
    return rv;
  }

  stream->conn = conn;

  rv = nghttp3_map_insert(&conn->streams, &stream->me);
  if (rv != 0) {
    nghttp3_stream_del(stream);
    return rv;
  }

  ++conn->next_seq;
  *pstream = stream;

  return 0;
}

int nghttp3_conn_create_placeholder(nghttp3_conn *conn,
                                    nghttp3_placeholder **pph, int64_t ph_id,
                                    uint32_t weight, nghttp3_tnode *parent) {
  nghttp3_placeholder *ph;
  int rv;

  rv = nghttp3_placeholder_new(&ph, ph_id, conn->next_seq, weight, parent,
                               conn->mem);
  if (rv != 0) {
    return rv;
  }

  rv = nghttp3_map_insert(&conn->placeholders, &ph->me);
  if (rv != 0) {
    nghttp3_placeholder_del(ph, conn->mem);
    return rv;
  }

  ++conn->next_seq;
  *pph = ph;

  return 0;
}

nghttp3_stream *nghttp3_conn_find_stream(nghttp3_conn *conn,
                                         int64_t stream_id) {
  nghttp3_map_entry *me;

  me = nghttp3_map_find(&conn->streams, (key_type)stream_id);
  if (me == NULL) {
    return NULL;
  }

  return nghttp3_struct_of(me, nghttp3_stream, me);
}

nghttp3_placeholder *nghttp3_conn_find_placeholder(nghttp3_conn *conn,
                                                   int64_t ph_id) {
  nghttp3_map_entry *me;

  me = nghttp3_map_find(&conn->placeholders, (key_type)ph_id);
  if (me == NULL) {
    return NULL;
  }

  return nghttp3_struct_of(me, nghttp3_placeholder, me);
}

int nghttp3_conn_bind_control_stream(nghttp3_conn *conn, int64_t stream_id) {
  nghttp3_stream *stream;
  nghttp3_frame_entry frent;
  int rv;

  if (conn->tx.ctrl) {
    return NGHTTP3_ERR_INVALID_STATE;
  }

  rv = nghttp3_conn_create_stream(conn, &stream, stream_id);
  if (rv != 0) {
    return rv;
  }

  stream->type = NGHTTP3_STREAM_TYPE_CONTROL;

  conn->tx.ctrl = stream;

  rv = nghttp3_stream_write_stream_type(stream);
  if (rv != 0) {
    return rv;
  }

  frent.fr.hd.type = NGHTTP3_FRAME_SETTINGS;
  frent.aux.settings.local_settings = &conn->local.settings;

  return nghttp3_stream_frq_add(stream, &frent);
}

int nghttp3_conn_bind_qpack_streams(nghttp3_conn *conn, int64_t qenc_stream_id,
                                    int64_t qdec_stream_id) {
  nghttp3_stream *stream;
  int rv;

  if (conn->tx.qenc || conn->tx.qdec) {
    return NGHTTP3_ERR_INVALID_STATE;
  }

  rv = nghttp3_conn_create_stream(conn, &stream, qenc_stream_id);
  if (rv != 0) {
    return rv;
  }

  stream->type = NGHTTP3_STREAM_TYPE_QPACK_ENCODER;

  conn->tx.qenc = stream;

  rv = nghttp3_stream_write_stream_type(stream);
  if (rv != 0) {
    return rv;
  }

  rv = nghttp3_conn_create_stream(conn, &stream, qdec_stream_id);
  if (rv != 0) {
    return rv;
  }

  stream->type = NGHTTP3_STREAM_TYPE_QPACK_DECODER;

  conn->tx.qdec = stream;

  return nghttp3_stream_write_stream_type(stream);
}

static ssize_t conn_writev_stream(nghttp3_conn *conn, int64_t *pstream_id,
                                  int *pfin, nghttp3_vec *vec, size_t veccnt,
                                  nghttp3_stream *stream) {
  int rv;
  ssize_t n;

  assert(veccnt > 0);

  rv = nghttp3_stream_fill_outq(stream);
  if (rv != 0) {
    return rv;
  }

  if (!nghttp3_stream_uni(stream->stream_id) && conn->tx.qenc &&
      !nghttp3_stream_is_blocked(conn->tx.qenc)) {
    n = nghttp3_stream_writev(conn->tx.qenc, pfin, vec, veccnt);
    if (n < 0) {
      return n;
    }
    if (n) {
      *pstream_id = conn->tx.qenc->stream_id;
      return n;
    }
  }

  n = nghttp3_stream_writev(stream, pfin, vec, veccnt);
  if (n < 0) {
    return n;
  }
  if (n == 0) {
    return 0;
  }

  *pstream_id = stream->stream_id;

  return n;
}

ssize_t nghttp3_conn_writev_stream(nghttp3_conn *conn, int64_t *pstream_id,
                                   int *pfin, nghttp3_vec *vec, size_t veccnt) {
  ssize_t ncnt;
  nghttp3_stream *stream;

  *pfin = 0;

  if (veccnt == 0) {
    return 0;
  }

  if (conn->tx.ctrl && !nghttp3_stream_is_blocked(conn->tx.ctrl)) {
    ncnt =
        conn_writev_stream(conn, pstream_id, pfin, vec, veccnt, conn->tx.ctrl);
    if (ncnt) {
      return ncnt;
    }
  }

  if (conn->tx.qdec && !nghttp3_stream_is_blocked(conn->tx.qdec)) {
    ncnt =
        conn_writev_stream(conn, pstream_id, pfin, vec, veccnt, conn->tx.qdec);
    if (ncnt) {
      return ncnt;
    }
  }

  if (conn->tx.qenc && !nghttp3_stream_is_blocked(conn->tx.qenc)) {
    ncnt =
        conn_writev_stream(conn, pstream_id, pfin, vec, veccnt, conn->tx.qenc);
    if (ncnt) {
      return ncnt;
    }
  }

  stream = nghttp3_conn_get_next_tx_stream(conn);
  if (stream == NULL) {
    return 0;
  }

  ncnt = conn_writev_stream(conn, pstream_id, pfin, vec, veccnt, stream);
  if (ncnt < 0) {
    return ncnt;
  }

  if (!nghttp3_stream_require_schedule(stream)) {
    nghttp3_stream_unschedule(stream);
  }

  return ncnt;
}

nghttp3_stream *nghttp3_conn_get_next_tx_stream(nghttp3_conn *conn) {
  nghttp3_tnode *node = nghttp3_tnode_get_next(&conn->root);

  if (node == NULL) {
    return NULL;
  }

  return nghttp3_struct_of(node, nghttp3_stream, node);
}

int nghttp3_conn_add_write_offset(nghttp3_conn *conn, int64_t stream_id,
                                  size_t n) {
  nghttp3_stream *stream = nghttp3_conn_find_stream(conn, stream_id);
  int rv;

  if (stream == NULL) {
    return NGHTTP3_ERR_INVALID_ARGUMENT;
  }

  rv = nghttp3_stream_add_outq_offset(stream, n);
  if (rv != 0) {
    return rv;
  }

  stream->unscheduled_nwrite += n;
  if (nghttp3_stream_is_blocked(stream)) {
    return 0;
  }

  if ((!nghttp3_stream_uni(stream_id) ||
       stream->type == NGHTTP3_STREAM_TYPE_PUSH) &&
      nghttp3_stream_require_schedule(stream)) {
    assert(stream->type == 0);

    return nghttp3_stream_schedule(stream);
  }

  return 0;
}

int nghttp3_conn_add_ack_offset(nghttp3_conn *conn, int64_t stream_id,
                                size_t n) {
  nghttp3_stream *stream = nghttp3_conn_find_stream(conn, stream_id);

  if (stream == NULL) {
    return NGHTTP3_ERR_INVALID_ARGUMENT;
  }

  return nghttp3_stream_add_ack_offset(stream, n);
}

static int conn_submit_headers_data(nghttp3_conn *conn, nghttp3_stream *stream,
                                    const nghttp3_nv *nva, size_t nvlen,
                                    const nghttp3_data_reader *dr) {
  int rv;
  nghttp3_nv *nnva;
  nghttp3_frame_entry frent;

  rv = nghttp3_nva_copy(&nnva, nva, nvlen, conn->mem);
  if (rv != 0) {
    return rv;
  }

  frent.fr.hd.type = NGHTTP3_FRAME_HEADERS;
  frent.fr.headers.nva = nnva;
  frent.fr.headers.nvlen = nvlen;

  rv = nghttp3_stream_frq_add(stream, &frent);
  if (rv != 0) {
    nghttp3_nva_del(nnva, conn->mem);
    return rv;
  }

  if (dr) {
    frent.fr.hd.type = NGHTTP3_FRAME_DATA;
    frent.aux.data.dr = *dr;

    rv = nghttp3_stream_frq_add(stream, &frent);
    if (rv != 0) {
      return rv;
    }
  }

  if (nghttp3_stream_require_schedule(stream)) {
    return nghttp3_stream_schedule(stream);
  }

  return 0;
}

int nghttp3_conn_submit_request(nghttp3_conn *conn, int64_t stream_id,
                                const nghttp3_priority *pri,
                                const nghttp3_nv *nva, size_t nvlen,
                                const nghttp3_data_reader *dr,
                                void *stream_user_data) {
  nghttp3_stream *stream;
  int rv;
  nghttp3_frame_entry frent;

  assert(conn->tx.qenc);

  if (conn->server) {
    return NGHTTP3_ERR_INVALID_STATE;
  }

  /* TODO Should we check that stream_id is client stream_id? */
  /* TODO Check GOAWAY last stream ID */
  if (nghttp3_stream_uni(stream_id)) {
    return NGHTTP3_ERR_INVALID_ARGUMENT;
  }

  if (pri != NULL) {
    if (pri->weight < 1 || 256 < pri->weight) {
      return NGHTTP3_ERR_INVALID_ARGUMENT;
    }

    switch (pri->elem_dep_type) {
    case NGHTTP3_ELEM_DEP_TYPE_REQUEST:
      if (pri->elem_dep_id >= stream_id) {
        return NGHTTP3_ERR_INVALID_ARGUMENT;
      }
      break;
    case NGHTTP3_ELEM_DEP_TYPE_PLACEHOLDER:
      if ((uint64_t)pri->elem_dep_id >=
          conn->remote.settings.num_placeholders) {
        return NGHTTP3_ERR_INVALID_ARGUMENT;
      }
      break;
    default:
      break;
    }
  }

  stream = nghttp3_conn_find_stream(conn, stream_id);
  if (stream != NULL) {
    return NGHTTP3_ERR_INVALID_ARGUMENT;
  }

  rv = nghttp3_conn_create_stream(conn, &stream, stream_id);
  if (rv != 0) {
    return rv;
  }
  stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_INITIAL;
  stream->tx.hstate = NGHTTP3_HTTP_STATE_REQ_END;
  stream->user_data = stream_user_data;

  if (pri) {
    frent.fr.hd.type = NGHTTP3_FRAME_PRIORITY;
    frent.fr.priority.pt = NGHTTP3_PRI_ELEM_TYPE_CURRENT;
    frent.fr.priority.dt = pri->elem_dep_type;
    frent.fr.priority.pri_elem_id = 0;
    frent.fr.priority.elem_dep_id = pri->elem_dep_id;

    rv = nghttp3_stream_frq_add(stream, &frent);
    if (rv != 0) {
      return rv;
    }
  }

  return conn_submit_headers_data(conn, stream, nva, nvlen, dr);
}

int nghttp3_conn_submit_response(nghttp3_conn *conn, int64_t stream_id,
                                 const nghttp3_nv *nva, size_t nvlen,
                                 const nghttp3_data_reader *dr) {
  nghttp3_stream *stream;

  assert(conn->tx.qenc);

  if (!conn->server) {
    return NGHTTP3_ERR_INVALID_STATE;
  }

  stream = nghttp3_conn_find_stream(conn, stream_id);
  if (stream == NULL) {
    return NGHTTP3_ERR_INVALID_ARGUMENT;
  }

  return conn_submit_headers_data(conn, stream, nva, nvlen, dr);
}

int nghttp3_conn_block_stream(nghttp3_conn *conn, int64_t stream_id) {
  nghttp3_stream *stream = nghttp3_conn_find_stream(conn, stream_id);

  if (stream == NULL) {
    return NGHTTP3_ERR_INVALID_ARGUMENT;
  }

  stream->flags |= NGHTTP3_STREAM_FLAG_FC_BLOCKED;

  nghttp3_stream_unschedule(stream);

  return 0;
}

int nghttp3_conn_unblock_stream(nghttp3_conn *conn, int64_t stream_id) {
  nghttp3_stream *stream = nghttp3_conn_find_stream(conn, stream_id);

  if (stream == NULL) {
    return NGHTTP3_ERR_INVALID_ARGUMENT;
  }

  stream->flags &= (uint16_t)~NGHTTP3_STREAM_FLAG_FC_BLOCKED;

  if (nghttp3_stream_require_schedule(stream)) {
    return nghttp3_stream_ensure_scheduled(stream);
  }

  return 0;
}

int nghttp3_conn_resume_stream(nghttp3_conn *conn, int64_t stream_id) {
  nghttp3_stream *stream = nghttp3_conn_find_stream(conn, stream_id);

  if (stream == NULL) {
    return NGHTTP3_ERR_INVALID_ARGUMENT;
  }

  stream->flags &= (uint16_t)~NGHTTP3_STREAM_FLAG_READ_DATA_BLOCKED;

  if (nghttp3_stream_require_schedule(stream)) {
    return nghttp3_stream_ensure_scheduled(stream);
  }

  return 0;
}

int nghttp3_conn_close_stream(nghttp3_conn *conn, int64_t stream_id) {
  nghttp3_stream *stream = nghttp3_conn_find_stream(conn, stream_id);
  int rv;

  if (stream == NULL) {
    return NGHTTP3_ERR_INVALID_ARGUMENT;
  }

  if (nghttp3_stream_uni(stream_id) &&
      stream->type != NGHTTP3_STREAM_TYPE_PUSH) {
    return NGHTTP3_ERR_HTTP_CLOSED_CRITICAL_STREAM;
  }

  if (conn->callbacks.stream_close) {
    rv = conn->callbacks.stream_close(conn, stream_id, stream->user_data,
                                      conn->user_data);
    if (rv != 0) {
      return NGHTTP3_ERR_CALLBACK_FAILURE;
    }
  }

  rv = nghttp3_map_remove(&conn->streams, (key_type)stream_id);

  assert(rv);

  return 0;
}

int nghttp3_conn_reset_stream(nghttp3_conn *conn, int64_t stream_id) {
  return nghttp3_qpack_decoder_cancel_stream(&conn->qdec, stream_id);
}

int nghttp3_conn_qpack_blocked_streams_push(nghttp3_conn *conn,
                                            nghttp3_stream *stream) {
  assert(stream->qpack_blocked_pe.index == NGHTTP3_PQ_BAD_INDEX);

  return nghttp3_pq_push(&conn->qpack_blocked_streams,
                         &stream->qpack_blocked_pe);
}

void nghttp3_conn_qpack_blocked_streams_pop(nghttp3_conn *conn) {
  assert(!nghttp3_pq_empty(&conn->qpack_blocked_streams));
  nghttp3_pq_pop(&conn->qpack_blocked_streams);
}

void nghttp3_conn_set_max_client_stream_id_bidi(nghttp3_conn *conn,
                                                int64_t max_stream_id) {
  assert(conn->server);
  assert(conn->rx.max_client_stream_id_bidi <= max_stream_id);

  conn->rx.max_client_stream_id_bidi = max_stream_id;
}

void nghttp3_conn_settings_default(nghttp3_conn_settings *settings) {
  memset(settings, 0, sizeof(nghttp3_conn_settings));
  settings->max_header_list_size = NGHTTP3_VARINT_MAX;
}

int nghttp3_placeholder_new(nghttp3_placeholder **pph, int64_t ph_id,
                            uint64_t seq, uint32_t weight,
                            nghttp3_tnode *parent, const nghttp3_mem *mem) {
  nghttp3_placeholder *ph;
  nghttp3_node_id nid;

  ph = nghttp3_mem_calloc(mem, 1, sizeof(nghttp3_placeholder));
  if (ph == NULL) {
    return NGHTTP3_ERR_NOMEM;
  }

  nghttp3_tnode_init(
      &ph->node,
      nghttp3_node_id_init(&nid, NGHTTP3_NODE_ID_TYPE_PLACEHOLDER, ph_id), seq,
      weight, parent, mem);

  ph->me.key = (key_type)ph_id;

  *pph = ph;

  return 0;
}

void nghttp3_placeholder_del(nghttp3_placeholder *ph, const nghttp3_mem *mem) {
  nghttp3_mem_free(mem, ph);
}

nghttp3_priority *nghttp3_priority_init(nghttp3_priority *pri,
                                        nghttp3_elem_dep_type type,
                                        int64_t elem_dep_id, uint32_t weight) {
  pri->elem_dep_type = type;
  pri->elem_dep_id = elem_dep_id;
  pri->weight = weight;

  return pri;
}
