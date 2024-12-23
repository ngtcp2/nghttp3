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
#include "nghttp3_test_helper.h"

#include <string.h>
#include <assert.h>

#include "nghttp3_str.h"
#include "nghttp3_conv.h"

void nghttp3_write_frame(nghttp3_buf *dest, nghttp3_frame *fr) {
  switch (fr->hd.type) {
  case NGHTTP3_FRAME_SETTINGS:
    nghttp3_frame_write_settings_len(&fr->hd.length, &fr->settings);
    dest->last = nghttp3_frame_write_settings(dest->last, &fr->settings);
    break;
  case NGHTTP3_FRAME_GOAWAY:
    nghttp3_frame_write_goaway_len(&fr->hd.length, &fr->goaway);
    dest->last = nghttp3_frame_write_goaway(dest->last, &fr->goaway);
    break;
  case NGHTTP3_FRAME_PRIORITY_UPDATE:
  case NGHTTP3_FRAME_PRIORITY_UPDATE_PUSH_ID:
    nghttp3_frame_write_priority_update_len(&fr->hd.length,
                                            &fr->priority_update);
    dest->last =
      nghttp3_frame_write_priority_update(dest->last, &fr->priority_update);
    break;
  default:
    assert(0);
  }
}

void nghttp3_write_frame_qpack(nghttp3_buf *dest, nghttp3_qpack_encoder *qenc,
                               int64_t stream_id, nghttp3_frame *fr) {
  int rv;
  const nghttp3_nv *nva;
  size_t nvlen;
  nghttp3_buf pbuf, rbuf, ebuf;
  const nghttp3_mem *mem = nghttp3_mem_default();
  (void)rv;

  switch (fr->hd.type) {
  case NGHTTP3_FRAME_HEADERS:
    nva = fr->headers.nva;
    nvlen = fr->headers.nvlen;
    break;
  default:
    assert(0);
    abort();
  }

  nghttp3_buf_init(&pbuf);
  nghttp3_buf_init(&rbuf);
  nghttp3_buf_init(&ebuf);

  rv = nghttp3_qpack_encoder_encode(qenc, &pbuf, &rbuf, &ebuf, stream_id, nva,
                                    nvlen);
  assert(0 == rv);
  assert(0 == nghttp3_buf_len(&ebuf));

  fr->hd.length = (int64_t)(nghttp3_buf_len(&pbuf) + nghttp3_buf_len(&rbuf));

  dest->last = nghttp3_frame_write_hd(dest->last, &fr->hd);
  dest->last = nghttp3_cpymem(dest->last, pbuf.pos, nghttp3_buf_len(&pbuf));
  dest->last = nghttp3_cpymem(dest->last, rbuf.pos, nghttp3_buf_len(&rbuf));

  nghttp3_buf_free(&rbuf, mem);
  nghttp3_buf_free(&pbuf, mem);
}

void nghttp3_write_frame_qpack_dyn(nghttp3_buf *dest, nghttp3_buf *ebuf,
                                   nghttp3_qpack_encoder *qenc,
                                   int64_t stream_id, nghttp3_frame *fr) {
  int rv;
  const nghttp3_nv *nva;
  size_t nvlen;
  nghttp3_buf pbuf, rbuf;
  const nghttp3_mem *mem = nghttp3_mem_default();
  (void)rv;

  switch (fr->hd.type) {
  case NGHTTP3_FRAME_HEADERS:
    nva = fr->headers.nva;
    nvlen = fr->headers.nvlen;
    break;
  default:
    assert(0);
    abort();
  }

  nghttp3_buf_init(&pbuf);
  nghttp3_buf_init(&rbuf);

  rv = nghttp3_qpack_encoder_encode(qenc, &pbuf, &rbuf, ebuf, stream_id, nva,
                                    nvlen);
  assert(0 == rv);

  fr->hd.length = (int64_t)(nghttp3_buf_len(&pbuf) + nghttp3_buf_len(&rbuf));

  dest->last = nghttp3_frame_write_hd(dest->last, &fr->hd);
  dest->last = nghttp3_cpymem(dest->last, pbuf.pos, nghttp3_buf_len(&pbuf));
  dest->last = nghttp3_cpymem(dest->last, rbuf.pos, nghttp3_buf_len(&rbuf));

  nghttp3_buf_free(&rbuf, mem);
  nghttp3_buf_free(&pbuf, mem);
}

void nghttp3_write_frame_data(nghttp3_buf *dest, size_t len) {
  nghttp3_frame_data fr = {
    .hd =
      {
        .type = NGHTTP3_FRAME_DATA,
        .length = (int64_t)len,
      },
  };

  dest->last = nghttp3_frame_write_hd(dest->last, &fr.hd);
  memset(dest->last, 0, len);
  dest->last += len;
}
