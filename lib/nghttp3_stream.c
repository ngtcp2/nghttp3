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
#include "nghttp3_stream.h"

#include <string.h>
#include <assert.h>
#include <stdio.h>

#include "nghttp3_conv.h"
#include "nghttp3_macro.h"
#include "nghttp3_frame.h"
#include "nghttp3_conn.h"
#include "nghttp3_str.h"

int nghttp3_stream_new(nghttp3_stream **pstream, int64_t stream_id,
                       uint64_t seq, uint32_t weight, nghttp3_tnode *parent,
                       const nghttp3_stream_callbacks *callbacks,
                       const nghttp3_mem *mem) {
  int rv;
  nghttp3_stream *stream = nghttp3_mem_calloc(mem, 1, sizeof(nghttp3_stream));
  nghttp3_node_id nid;

  if (stream == NULL) {
    return NGHTTP3_ERR_NOMEM;
  }

  nghttp3_tnode_init(
      &stream->node,
      nghttp3_node_id_init(&nid, NGHTTP3_NODE_ID_TYPE_STREAM, stream_id), seq,
      weight, parent, mem);

  rv = nghttp3_ringbuf_init(&stream->frq, 16, sizeof(nghttp3_frame_entry), mem);
  if (rv != 0) {
    goto frq_init_fail;
  }

  rv = nghttp3_ringbuf_init(&stream->chunks, 16, sizeof(nghttp3_buf), mem);
  if (rv != 0) {
    goto chunks_init_fail;
  }

  rv = nghttp3_ringbuf_init(&stream->outq, 16, sizeof(nghttp3_typed_buf), mem);
  if (rv != 0) {
    goto outq_init_fail;
  }

  rv = nghttp3_ringbuf_init(&stream->inq, 16, sizeof(nghttp3_buf), mem);
  if (rv != 0) {
    goto inq_init_fail;
  }

  nghttp3_qpack_stream_context_init(&stream->qpack_sctx, stream_id, mem);

  stream->stream_id = stream_id;
  stream->me.key = (key_type)stream_id;
  stream->qpack_blocked_pe.index = NGHTTP3_PQ_BAD_INDEX;
  stream->mem = mem;

  if (callbacks) {
    stream->callbacks = *callbacks;
  }

  *pstream = stream;

  return 0;

inq_init_fail:
  nghttp3_ringbuf_free(&stream->outq);
outq_init_fail:
  nghttp3_ringbuf_free(&stream->chunks);
chunks_init_fail:
  nghttp3_ringbuf_free(&stream->frq);
frq_init_fail:
  nghttp3_mem_free(mem, stream);

  return rv;
}

static void delete_outq(nghttp3_ringbuf *outq, const nghttp3_mem *mem) {
  nghttp3_typed_buf *tbuf;
  size_t i, len = nghttp3_ringbuf_len(outq);

  for (i = 0; i < len; ++i) {
    tbuf = nghttp3_ringbuf_get(outq, i);
    if (tbuf->type == NGHTTP3_BUF_TYPE_PRIVATE) {
      nghttp3_buf_free(&tbuf->buf, mem);
    }
  }

  nghttp3_ringbuf_free(outq);
}

static void delete_chunks(nghttp3_ringbuf *chunks, const nghttp3_mem *mem) {
  nghttp3_buf *buf;
  size_t i, len = nghttp3_ringbuf_len(chunks);

  for (i = 0; i < len; ++i) {
    buf = nghttp3_ringbuf_get(chunks, i);
    nghttp3_buf_free(buf, mem);
  }

  nghttp3_ringbuf_free(chunks);
}

static void delete_frq(nghttp3_ringbuf *frq, const nghttp3_mem *mem) {
  nghttp3_frame_entry *frent;
  size_t i, len = nghttp3_ringbuf_len(frq);

  for (i = 0; i < len; ++i) {
    frent = nghttp3_ringbuf_get(frq, i);
    switch (frent->fr.hd.type) {
    case NGHTTP3_FRAME_HEADERS:
      nghttp3_frame_headers_free(&frent->fr.headers, mem);
      break;
    default:
      break;
    }
  }

  nghttp3_ringbuf_free(frq);
}

void nghttp3_stream_del(nghttp3_stream *stream) {
  if (stream == NULL) {
    return;
  }

  nghttp3_qpack_stream_context_free(&stream->qpack_sctx);
  delete_chunks(&stream->inq, stream->mem);
  delete_outq(&stream->outq, stream->mem);
  delete_chunks(&stream->chunks, stream->mem);
  delete_frq(&stream->frq, stream->mem);

  nghttp3_mem_free(stream->mem, stream);
}

void nghttp3_varint_read_state_reset(nghttp3_varint_read_state *rvint) {
  memset(rvint, 0, sizeof(*rvint));
}

void nghttp3_stream_read_state_reset(nghttp3_stream_read_state *rstate) {
  memset(rstate, 0, sizeof(*rstate));
}

ssize_t nghttp3_read_varint(nghttp3_varint_read_state *rvint,
                            const uint8_t *src, size_t srclen, int fin) {
  size_t nread = 0;
  size_t n;
  size_t i;

  assert(srclen > 0);

  if (rvint->left == 0) {
    assert(rvint->acc == 0);

    rvint->left = nghttp3_get_varint_len(src);
    if (rvint->left <= srclen) {
      rvint->acc = nghttp3_get_varint(&nread, src);
      rvint->left = 0;
      return (ssize_t)nread;
    }

    if (fin) {
      return NGHTTP3_ERR_INVALID_ARGUMENT;
    }

    rvint->acc = nghttp3_get_varint_fb(src);
    nread = 1;
    ++src;
    --srclen;
    --rvint->left;
  }

  n = nghttp3_min(rvint->left, srclen);

  for (i = 0; i < n; ++i) {
    rvint->acc = (rvint->acc << 8) + src[i];
  }

  rvint->left -= n;
  nread += n;

  if (fin && rvint->left) {
    return NGHTTP3_ERR_INVALID_ARGUMENT;
  }

  return (ssize_t)nread;
}

int nghttp3_stream_frq_add(nghttp3_stream *stream,
                           const nghttp3_frame_entry *frent) {
  nghttp3_ringbuf *frq = &stream->frq;
  nghttp3_frame_entry *dest;
  int rv;

  if (nghttp3_ringbuf_full(frq)) {
    rv = nghttp3_ringbuf_reserve(frq, nghttp3_ringbuf_len(frq) * 2);
    if (rv != 0) {
      return rv;
    }
  }

  dest = nghttp3_ringbuf_push_back(frq);
  *dest = *frent;

  return 0;
}

int nghttp3_stream_fill_outq(nghttp3_stream *stream) {
  nghttp3_ringbuf *frq = &stream->frq;
  nghttp3_frame_entry *frent;
  int rv;

  for (; nghttp3_ringbuf_len(frq) && !nghttp3_stream_outq_is_full(stream);) {
    frent = nghttp3_ringbuf_get(frq, 0);

    switch (frent->fr.hd.type) {
    case NGHTTP3_FRAME_SETTINGS:
      rv = nghttp3_stream_write_settings(stream, frent);
      if (rv != 0) {
        return rv;
      }
      break;
    case NGHTTP3_FRAME_HEADERS:
      rv = nghttp3_stream_write_headers(stream, frent);
      if (rv != 0) {
        return rv;
      }
      nghttp3_frame_headers_free(&frent->fr.headers, stream->mem);
      break;
    case NGHTTP3_FRAME_DATA:
      for (; !nghttp3_stream_outq_is_full(stream);) {
        rv = nghttp3_stream_write_data(stream, frent);
        if (rv != 0) {
          return rv;
        }
        if (stream->flags & NGHTTP3_STREAM_FLAG_DATA_EOF) {
          break;
        }
        if (stream->flags & NGHTTP3_STREAM_FLAG_READ_DATA_BLOCKED) {
          return 0;
        }
      }
      break;
    default:
      /* TODO Not implemented */
      break;
    }

    nghttp3_ringbuf_pop_front(frq);
  }

  return 0;
}

static void typed_buf_shared_init(nghttp3_typed_buf *tbuf,
                                  const nghttp3_buf *chunk) {
  nghttp3_typed_buf_init(tbuf, chunk, NGHTTP3_BUF_TYPE_SHARED);
  tbuf->buf.pos = tbuf->buf.last;
}

int nghttp3_stream_write_stream_type(nghttp3_stream *stream) {
  size_t len = nghttp3_put_varint_len(stream->type);
  nghttp3_buf *chunk;
  nghttp3_typed_buf tbuf;
  int rv;

  rv = nghttp3_stream_ensure_chunk(stream, len);
  if (rv != 0) {
    return rv;
  }

  chunk = nghttp3_stream_get_chunk(stream);
  typed_buf_shared_init(&tbuf, chunk);

  chunk->last = nghttp3_put_varint(chunk->last, stream->type);
  tbuf.buf.last = chunk->last;

  return nghttp3_stream_outq_add(stream, &tbuf);
}

int nghttp3_stream_write_settings(nghttp3_stream *stream,
                                  nghttp3_frame_entry *frent) {
  size_t payloadlen;
  size_t len;
  int rv;
  nghttp3_buf *chunk;
  nghttp3_typed_buf tbuf;
  struct {
    nghttp3_frame_settings settings;
    nghttp3_settings_entry iv[15];
  } fr;
  nghttp3_settings_entry *iv;
  nghttp3_conn_settings *local_settings = frent->aux.settings.local_settings;

  fr.settings.hd.type = NGHTTP3_FRAME_SETTINGS;
  iv = fr.settings.iv;
  /* TODO Don't send default value */
  iv[0].id = NGHTTP3_SETTINGS_ID_MAX_HEADER_LIST_SIZE;
  iv[0].value = (int64_t)local_settings->max_header_list_size;
  iv[1].id = NGHTTP3_SETTINGS_ID_NUM_PLACEHOLDERS;
  iv[1].value = (int64_t)local_settings->num_placeholders;
  iv[2].id = NGHTTP3_SETTINGS_ID_QPACK_MAX_TABLE_CAPACITY;
  iv[2].value = (int64_t)local_settings->qpack_max_table_capacity;
  iv[3].id = NGHTTP3_SETTINGS_ID_QPACK_BLOCKED_STREAMS;
  iv[3].value = (int64_t)local_settings->qpack_blocked_streams;
  fr.settings.niv = 4;

  len = nghttp3_frame_write_settings_len(&payloadlen, &fr.settings);

  rv = nghttp3_stream_ensure_chunk(stream, len);
  if (rv != 0) {
    return rv;
  }

  chunk = nghttp3_stream_get_chunk(stream);
  typed_buf_shared_init(&tbuf, chunk);

  fr.settings.hd.length = (int64_t)payloadlen;
  rv = nghttp3_frame_write_settings(chunk, &fr.settings);
  if (rv != 0) {
    return rv;
  }

  tbuf.buf.last = chunk->last;

  return nghttp3_stream_outq_add(stream, &tbuf);
}

int nghttp3_stream_write_headers(nghttp3_stream *stream,
                                 nghttp3_frame_entry *frent) {
  nghttp3_qpack_encoder *qenc;
  nghttp3_stream *qenc_stream;
  nghttp3_frame_headers *fr = &frent->fr.headers;
  nghttp3_buf pbuf, rbuf, ebuf;
  int rv;
  size_t len;
  nghttp3_buf *chunk;
  nghttp3_typed_buf tbuf;
  nghttp3_frame_hd hd;

  assert(stream->conn);

  qenc = &stream->conn->qenc;
  qenc_stream = stream->conn->tx.qenc;

  nghttp3_buf_init(&pbuf);
  nghttp3_buf_init(&rbuf);
  nghttp3_buf_init(&ebuf);

  rv = nghttp3_qpack_encoder_encode(qenc, &pbuf, &rbuf, &ebuf,
                                    stream->stream_id, fr->nva, fr->nvlen);
  if (rv != 0) {
    goto fail;
  }

  hd.type = NGHTTP3_FRAME_HEADERS;
  hd.length = (int64_t)(nghttp3_buf_len(&pbuf) + nghttp3_buf_len(&rbuf));

  len = nghttp3_frame_write_hd_len(&hd);

  rv = nghttp3_stream_ensure_chunk(stream, len);
  if (rv != 0) {
    goto fail;
  }

  chunk = nghttp3_stream_get_chunk(stream);
  typed_buf_shared_init(&tbuf, chunk);

  rv = nghttp3_frame_write_hd(chunk, &hd);
  if (rv != 0) {
    goto fail;
  }

  tbuf.buf.last = chunk->last;

  rv = nghttp3_stream_outq_add(stream, &tbuf);
  if (rv != 0) {
    goto fail;
  }

  nghttp3_typed_buf_init(&tbuf, &pbuf, NGHTTP3_BUF_TYPE_PRIVATE);
  rv = nghttp3_stream_outq_add(stream, &tbuf);
  if (rv != 0) {
    goto fail;
  }
  nghttp3_buf_init(&pbuf);

  if (nghttp3_buf_len(&rbuf)) {
    nghttp3_typed_buf_init(&tbuf, &rbuf, NGHTTP3_BUF_TYPE_PRIVATE);
    rv = nghttp3_stream_outq_add(stream, &tbuf);
    if (rv != 0) {
      goto fail;
    }
    nghttp3_buf_init(&rbuf);
  }

  if (nghttp3_buf_len(&ebuf)) {
    assert(qenc_stream);

    nghttp3_typed_buf_init(&tbuf, &ebuf, NGHTTP3_BUF_TYPE_PRIVATE);
    rv = nghttp3_stream_outq_add(qenc_stream, &tbuf);
    if (rv != 0) {
      nghttp3_buf_free(&ebuf, stream->mem);
      return rv;
    }
    nghttp3_buf_init(&ebuf);
  }

  assert(0 == nghttp3_buf_len(&pbuf));
  assert(0 == nghttp3_buf_len(&rbuf));
  assert(0 == nghttp3_buf_len(&ebuf));

  return 0;

fail:
  nghttp3_buf_free(&ebuf, stream->mem);
  nghttp3_buf_free(&rbuf, stream->mem);
  nghttp3_buf_free(&pbuf, stream->mem);

  return rv;
}

int nghttp3_stream_write_data(nghttp3_stream *stream,
                              nghttp3_frame_entry *frent) {
  int rv;
  size_t len;
  nghttp3_typed_buf tbuf;
  nghttp3_buf buf;
  nghttp3_buf *chunk;
  nghttp3_read_data_callback read_data = frent->aux.data.dr.read_data;
  nghttp3_conn *conn = stream->conn;
  const uint8_t *data = NULL;
  size_t datalen = 0;
  uint32_t flags = 0;
  nghttp3_frame_hd hd;

  assert(!(stream->flags & NGHTTP3_STREAM_FLAG_DATA_EOF));
  assert(!(stream->flags & NGHTTP3_STREAM_FLAG_READ_DATA_BLOCKED));
  assert(read_data);
  assert(conn);

  rv = read_data(conn, stream->stream_id, &data, &datalen, &flags,
                 stream->user_data, conn->user_data);
  if (rv != 0) {
    if (rv == NGHTTP3_ERR_WOULDBLOCKED) {
      stream->flags |= NGHTTP3_STREAM_FLAG_READ_DATA_BLOCKED;
      return 0;
    }
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }

  assert(datalen || flags & NGHTTP3_DATA_FLAG_EOF);

  if (flags & NGHTTP3_DATA_FLAG_EOF) {
    stream->flags |= NGHTTP3_STREAM_FLAG_DATA_EOF;
  }

  hd.type = NGHTTP3_FRAME_DATA;
  hd.length = (int64_t)datalen;

  len = nghttp3_frame_write_hd_len(&hd);

  rv = nghttp3_stream_ensure_chunk(stream, len);
  if (rv != 0) {
    return rv;
  }

  chunk = nghttp3_stream_get_chunk(stream);
  typed_buf_shared_init(&tbuf, chunk);

  rv = nghttp3_frame_write_hd(chunk, &hd);
  if (rv != 0) {
    return rv;
  }

  tbuf.buf.last = chunk->last;

  rv = nghttp3_stream_outq_add(stream, &tbuf);
  if (rv != 0) {
    return rv;
  }

  nghttp3_buf_wrap_init(&buf, (uint8_t *)data, datalen);
  buf.last = buf.end;
  nghttp3_typed_buf_init(&tbuf, &buf, NGHTTP3_BUF_TYPE_ALIEN);
  rv = nghttp3_stream_outq_add(stream, &tbuf);
  if (rv != 0) {
    return rv;
  }

  return 0;
}

int nghttp3_stream_outq_is_full(nghttp3_stream *stream) {
  /* TODO Verify that the limit is reasonable. */
  return nghttp3_ringbuf_len(&stream->outq) >= 32;
}

int nghttp3_stream_outq_add(nghttp3_stream *stream,
                            const nghttp3_typed_buf *tbuf) {
  nghttp3_ringbuf *outq = &stream->outq;
  int rv;
  nghttp3_typed_buf *dest;
  size_t len = nghttp3_ringbuf_len(outq);

  if (len) {
    dest = nghttp3_ringbuf_get(outq, len - 1);
    if (dest->type == tbuf->type && dest->type == NGHTTP3_BUF_TYPE_SHARED &&
        dest->buf.begin == tbuf->buf.begin && dest->buf.last == tbuf->buf.pos) {
      dest->buf.last = tbuf->buf.last;
      dest->buf.end = tbuf->buf.end;
      return 0;
    }
  }

  if (nghttp3_ringbuf_full(outq)) {
    rv = nghttp3_ringbuf_reserve(outq, len * 2);
    if (rv != 0) {
      return rv;
    }
  }

  dest = nghttp3_ringbuf_push_back(outq);
  *dest = *tbuf;

  return 0;
}

int nghttp3_stream_ensure_chunk(nghttp3_stream *stream, size_t need) {
  nghttp3_ringbuf *chunks = &stream->chunks;
  nghttp3_buf *chunk;
  size_t len = nghttp3_ringbuf_len(chunks);
  uint8_t *p;
  int rv;

  if (len) {
    chunk = nghttp3_ringbuf_get(chunks, len - 1);
    if (nghttp3_buf_left(chunk) >= need) {
      return 0;
    }
  }

  assert(NGHTTP3_STREAM_CHUNK_SIZE >= need);

  p = nghttp3_mem_malloc(stream->mem, NGHTTP3_STREAM_CHUNK_SIZE);
  if (p == NULL) {
    return NGHTTP3_ERR_NOMEM;
  }

  if (nghttp3_ringbuf_full(chunks)) {
    rv = nghttp3_ringbuf_reserve(chunks, len * 2);
    if (rv != 0) {
      return rv;
    }
  }

  chunk = nghttp3_ringbuf_push_back(chunks);
  nghttp3_buf_wrap_init(chunk, p, NGHTTP3_STREAM_CHUNK_SIZE);

  return 0;
}

nghttp3_buf *nghttp3_stream_get_chunk(nghttp3_stream *stream) {
  nghttp3_ringbuf *chunks = &stream->chunks;
  size_t len = nghttp3_ringbuf_len(chunks);

  assert(len);

  return nghttp3_ringbuf_get(chunks, len - 1);
}

int nghttp3_stream_is_blocked(nghttp3_stream *stream) {
  return (stream->flags & NGHTTP3_STREAM_FLAG_FC_BLOCKED) ||
         (stream->flags & NGHTTP3_STREAM_FLAG_READ_DATA_BLOCKED);
}

int nghttp3_stream_require_schedule(nghttp3_stream *stream) {
  return (nghttp3_ringbuf_len(&stream->outq) ||
          nghttp3_ringbuf_len(&stream->frq)) &&
         !nghttp3_stream_is_blocked(stream);
}

ssize_t nghttp3_stream_writev(nghttp3_stream *stream, int *pfin,
                              nghttp3_vec *vec, size_t veccnt) {
  nghttp3_ringbuf *outq = &stream->outq;
  size_t len = nghttp3_ringbuf_len(outq);
  size_t i;
  size_t offset = stream->outq_offset;
  size_t buflen;
  nghttp3_vec *vbegin = vec, *vend = vec + veccnt;
  nghttp3_typed_buf *tbuf;

  assert(veccnt > 0);

  for (i = stream->outq_idx; i < len; ++i) {
    tbuf = nghttp3_ringbuf_get(outq, i);
    buflen = nghttp3_buf_len(&tbuf->buf);
    if (offset >= buflen) {
      offset -= buflen;
      continue;
    }

    vec->base = tbuf->buf.pos + offset;
    vec->len = buflen - offset;
    ++vec;
    ++i;
    break;
  }

  for (; i < len && vec != vend; ++i, ++vec) {
    tbuf = nghttp3_ringbuf_get(outq, i);
    vec->base = tbuf->buf.pos;
    vec->len = nghttp3_buf_len(&tbuf->buf);
  }

  /* TODO Rework this if we have finished implementing HTTP
     messaging */
  *pfin = i == len && (stream->flags & NGHTTP3_STREAM_FLAG_DATA_EOF);

  return vec - vbegin;
}

int nghttp3_stream_add_outq_offset(nghttp3_stream *stream, size_t n) {
  nghttp3_ringbuf *outq = &stream->outq;
  size_t i;
  size_t len = nghttp3_ringbuf_len(outq);
  size_t offset = stream->outq_offset + n;
  size_t buflen;
  nghttp3_typed_buf *tbuf;

  for (i = stream->outq_idx; i < len; ++i) {
    tbuf = nghttp3_ringbuf_get(outq, i);
    buflen = nghttp3_buf_len(&tbuf->buf);
    if (offset == buflen && tbuf->type == NGHTTP3_BUF_TYPE_SHARED &&
        i + 1 == len) {
      /* Stop iterate here so that we can coalesces extra buffer to
         this entry */
      break;
    }
    if (offset >= buflen) {
      offset -= buflen;
      continue;
    }

    break;
  }

  assert(i < len || offset == 0);

  stream->outq_idx = i;
  stream->outq_offset = offset;

  return 0;
}

static int stream_pop_outq_entry(nghttp3_stream *stream,
                                 nghttp3_typed_buf *tbuf) {
  nghttp3_ringbuf *chunks = &stream->chunks;
  nghttp3_buf *chunk;

  switch (tbuf->type) {
  case NGHTTP3_BUF_TYPE_PRIVATE:
    nghttp3_buf_free(&tbuf->buf, stream->mem);
    break;
  case NGHTTP3_BUF_TYPE_ALIEN:
    break;
  default:
    assert(nghttp3_ringbuf_len(chunks));

    chunk = nghttp3_ringbuf_get(chunks, 0);

    assert(chunk->begin == tbuf->buf.begin);
    assert(chunk->end == tbuf->buf.end);

    if (chunk->last == tbuf->buf.last) {
      nghttp3_buf_free(chunk, stream->mem);
      nghttp3_ringbuf_pop_front(chunks);
    }
  };

  nghttp3_ringbuf_pop_front(&stream->outq);

  return 0;
}

int nghttp3_stream_add_ack_offset(nghttp3_stream *stream, size_t n) {
  nghttp3_ringbuf *outq = &stream->outq;
  size_t offset = stream->ack_offset + n;
  size_t buflen;
  size_t npopped = 0;
  size_t nack;
  nghttp3_typed_buf *tbuf;
  int rv;

  for (; nghttp3_ringbuf_len(outq);) {
    tbuf = nghttp3_ringbuf_get(outq, 0);
    buflen = nghttp3_buf_len(&tbuf->buf);

    if (tbuf->type == NGHTTP3_BUF_TYPE_ALIEN) {
      nack = nghttp3_min(offset, buflen) - stream->ack_done;
      if (stream->callbacks.acked_data) {
        rv = stream->callbacks.acked_data(stream, stream->stream_id, nack,
                                          stream->user_data);
        if (rv != 0) {
          return NGHTTP3_ERR_CALLBACK_FAILURE;
        }
      }
      stream->ack_done += nack;
    }

    if (offset >= buflen) {
      rv = stream_pop_outq_entry(stream, tbuf);
      if (rv != 0) {
        return rv;
      }

      offset -= buflen;
      ++npopped;
      stream->ack_done = 0;

      if (stream->outq_idx + 1 == npopped) {
        stream->outq_offset = 0;
        break;
      }

      continue;
    }

    break;
  }

  assert(stream->outq_idx + 1 >= npopped);
  if (stream->outq_idx >= npopped) {
    stream->outq_idx -= npopped;
  } else {
    stream->outq_idx = 0;
  }

  stream->ack_offset = offset;

  return 0;
}

int nghttp3_stream_schedule(nghttp3_stream *stream) {
  int rv;

  rv = nghttp3_tnode_schedule(&stream->node, stream->unscheduled_nwrite);
  if (rv != 0) {
    return rv;
  }

  stream->unscheduled_nwrite = 0;

  return 0;
}

int nghttp3_stream_ensure_scheduled(nghttp3_stream *stream) {
  if (nghttp3_tnode_is_scheduled(&stream->node)) {
    return 0;
  }

  return nghttp3_stream_schedule(stream);
}

void nghttp3_stream_unschedule(nghttp3_stream *stream) {
  nghttp3_tnode_unschedule(&stream->node);
}

int nghttp3_stream_buffer_data(nghttp3_stream *stream, const uint8_t *data,
                               size_t datalen) {
  nghttp3_ringbuf *inq = &stream->inq;
  size_t len = nghttp3_ringbuf_len(inq);
  nghttp3_buf *buf;
  size_t nwrite;
  uint8_t *rawbuf;
  size_t bufleft;
  int rv;

  if (len) {
    buf = nghttp3_ringbuf_get(inq, len - 1);
    bufleft = nghttp3_buf_left(buf);
    nwrite = nghttp3_min(datalen, bufleft);
    buf->last = nghttp3_cpymem(buf->last, data, nwrite);
    data += nwrite;
    datalen -= nwrite;
    if (len == 0) {
      return 0;
    }
  }

  for (; datalen;) {
    if (nghttp3_ringbuf_full(inq)) {
      rv = nghttp3_ringbuf_reserve(inq, nghttp3_ringbuf_len(inq) * 2);
      if (rv != 0) {
        return rv;
      }
    }

    rawbuf = nghttp3_mem_malloc(stream->mem, 16384);
    if (rawbuf == NULL) {
      return NGHTTP3_ERR_NOMEM;
    }

    buf = nghttp3_ringbuf_push_back(inq);
    nghttp3_buf_wrap_init(buf, rawbuf, 16384);
    bufleft = nghttp3_buf_left(buf);
    nwrite = nghttp3_min(datalen, bufleft);
    buf->last = nghttp3_cpymem(buf->last, data, nwrite);
    data += nwrite;
    datalen -= nwrite;
  }

  return 0;
}

int nghttp3_stream_transit_rx_http_state(nghttp3_stream *stream,
                                         nghttp3_stream_http_event event) {
  switch (stream->rx.hstate) {
  case NGHTTP3_HTTP_STATE_NONE:
    return NGHTTP3_ERR_HTTP_INTERNAL;
  case NGHTTP3_HTTP_STATE_REQ_INITIAL:
    switch (event) {
    case NGHTTP3_HTTP_EVENT_HEADERS_BEGIN:
      stream->rx.hstate = NGHTTP3_HTTP_STATE_REQ_HEADERS_BEGIN;
      return 0;
    case NGHTTP3_HTTP_EVENT_PRIORITY_BEGIN:
      stream->rx.hstate = NGHTTP3_HTTP_STATE_REQ_PRIORITY_BEGIN;
      return 0;
    default:
      return NGHTTP3_ERR_HTTP_UNEXPECTED_FRAME;
    }
  case NGHTTP3_HTTP_STATE_REQ_PRIORITY_BEGIN:
    if (event != NGHTTP3_HTTP_EVENT_PRIORITY_END) {
      return NGHTTP3_ERR_HTTP_GENERAL_PROTOCOL;
    }
    stream->rx.hstate = NGHTTP3_HTTP_STATE_REQ_PRIORITY_END;
    return 0;
  case NGHTTP3_HTTP_STATE_REQ_PRIORITY_END:
    if (event != NGHTTP3_HTTP_EVENT_HEADERS_BEGIN) {
      return NGHTTP3_ERR_HTTP_UNEXPECTED_FRAME;
    }
    stream->rx.hstate = NGHTTP3_HTTP_STATE_REQ_HEADERS_BEGIN;
    return 0;
  case NGHTTP3_HTTP_STATE_REQ_HEADERS_BEGIN:
    if (event != NGHTTP3_HTTP_EVENT_HEADERS_END) {
      return NGHTTP3_ERR_HTTP_GENERAL_PROTOCOL;
    }
    stream->rx.hstate = NGHTTP3_HTTP_STATE_REQ_HEADERS_END;
    return 0;
  case NGHTTP3_HTTP_STATE_REQ_HEADERS_END:
    switch (event) {
    case NGHTTP3_HTTP_EVENT_DATA_BEGIN:
      stream->rx.hstate = NGHTTP3_HTTP_STATE_REQ_DATA_BEGIN;
      return 0;
    case NGHTTP3_HTTP_EVENT_MSG_END:
      stream->rx.hstate = NGHTTP3_HTTP_STATE_REQ_END;
      return 0;
    default:
      return NGHTTP3_ERR_HTTP_UNEXPECTED_FRAME;
    }
  case NGHTTP3_HTTP_STATE_REQ_DATA_BEGIN:
    if (event != NGHTTP3_HTTP_EVENT_DATA_END) {
      return NGHTTP3_ERR_HTTP_GENERAL_PROTOCOL;
    }
    stream->rx.hstate = NGHTTP3_HTTP_STATE_REQ_DATA_END;
    return 0;
  case NGHTTP3_HTTP_STATE_REQ_DATA_END:
    switch (event) {
    case NGHTTP3_HTTP_EVENT_DATA_BEGIN:
      stream->rx.hstate = NGHTTP3_HTTP_STATE_REQ_DATA_BEGIN;
      return 0;
    case NGHTTP3_HTTP_EVENT_HEADERS_BEGIN:
      stream->rx.hstate = NGHTTP3_HTTP_STATE_REQ_TRAILERS_BEGIN;
      return 0;
    case NGHTTP3_HTTP_EVENT_MSG_END:
      stream->rx.hstate = NGHTTP3_HTTP_STATE_REQ_END;
      return 0;
    default:
      return NGHTTP3_ERR_HTTP_UNEXPECTED_FRAME;
    }
  case NGHTTP3_HTTP_STATE_REQ_TRAILERS_BEGIN:
    if (event != NGHTTP3_HTTP_EVENT_HEADERS_END) {
      return NGHTTP3_ERR_HTTP_GENERAL_PROTOCOL;
    }
    stream->rx.hstate = NGHTTP3_HTTP_STATE_REQ_TRAILERS_END;
    return 0;
  case NGHTTP3_HTTP_STATE_REQ_TRAILERS_END:
    if (event != NGHTTP3_HTTP_EVENT_MSG_END) {
      /* TODO Should ignore unexpected frame in this state as per
         spec. */
      return NGHTTP3_ERR_HTTP_UNEXPECTED_FRAME;
    }
    stream->rx.hstate = NGHTTP3_HTTP_STATE_REQ_END;
    return 0;
  case NGHTTP3_HTTP_STATE_REQ_END:
    return NGHTTP3_ERR_HTTP_GENERAL_PROTOCOL;
  case NGHTTP3_HTTP_STATE_RESP_INITIAL:
    if (event != NGHTTP3_HTTP_EVENT_HEADERS_BEGIN) {
      return NGHTTP3_ERR_HTTP_UNEXPECTED_FRAME;
    }
    stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_HEADERS_BEGIN;
    return 0;
  case NGHTTP3_HTTP_STATE_RESP_HEADERS_BEGIN:
    if (event != NGHTTP3_HTTP_EVENT_HEADERS_END) {
      return NGHTTP3_ERR_HTTP_GENERAL_PROTOCOL;
    }
    stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_HEADERS_END;
    return 0;
  case NGHTTP3_HTTP_STATE_RESP_HEADERS_END:
    /* TODO Support info (non-final) response */
    switch (event) {
    case NGHTTP3_HTTP_EVENT_DATA_BEGIN:
      stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_DATA_BEGIN;
      return 0;
    case NGHTTP3_HTTP_EVENT_MSG_END:
      stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_END;
      return 0;
    default:
      return NGHTTP3_ERR_HTTP_UNEXPECTED_FRAME;
    }
  case NGHTTP3_HTTP_STATE_RESP_DATA_BEGIN:
    if (event != NGHTTP3_HTTP_EVENT_DATA_END) {
      return NGHTTP3_ERR_HTTP_GENERAL_PROTOCOL;
    }
    stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_DATA_END;
    return 0;
  case NGHTTP3_HTTP_STATE_RESP_DATA_END:
    switch (event) {
    case NGHTTP3_HTTP_EVENT_DATA_BEGIN:
      stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_DATA_BEGIN;
      return 0;
    case NGHTTP3_HTTP_EVENT_HEADERS_BEGIN:
      stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_TRAILERS_BEGIN;
      return 0;
    case NGHTTP3_HTTP_EVENT_MSG_END:
      stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_END;
      return 0;
    default:
      return NGHTTP3_ERR_HTTP_UNEXPECTED_FRAME;
    }
  case NGHTTP3_HTTP_STATE_RESP_TRAILERS_BEGIN:
    if (event != NGHTTP3_HTTP_EVENT_HEADERS_END) {
      return NGHTTP3_ERR_HTTP_GENERAL_PROTOCOL;
    }
    stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_TRAILERS_END;
    return 0;
  case NGHTTP3_HTTP_STATE_RESP_TRAILERS_END:
    if (event != NGHTTP3_HTTP_EVENT_MSG_END) {
      /* TODO Should ignore unexpected frame in this state as per
         spec. */
      return NGHTTP3_ERR_HTTP_UNEXPECTED_FRAME;
    }
    stream->rx.hstate = NGHTTP3_HTTP_STATE_RESP_END;
    return 0;
  case NGHTTP3_HTTP_STATE_RESP_END:
    return NGHTTP3_ERR_HTTP_GENERAL_PROTOCOL;
  default:
    assert(0);
  }
}

int nghttp3_stream_empty_headers_allowed(nghttp3_stream *stream) {
  switch (stream->rx.hstate) {
  case NGHTTP3_HTTP_STATE_REQ_TRAILERS_BEGIN:
  case NGHTTP3_HTTP_STATE_RESP_TRAILERS_BEGIN:
    return 0;
  default:
    return NGHTTP3_ERR_HTTP_GENERAL_PROTOCOL;
  }
}

int nghttp3_stream_uni(int64_t stream_id) { return (stream_id & 0x2) != 0; }

int nghttp3_client_stream_bidi(int64_t stream_id) {
  return (stream_id & 0x3) == 0;
}
