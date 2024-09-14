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
#ifndef NGHTTP3_TEST_HELPER
#define NGHTTP3_TEST_HELPER

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* defined(HAVE_CONFIG_H) */

#include "nghttp3_buf.h"
#include "nghttp3_frame.h"
#include "nghttp3_qpack.h"

#define MAKE_NV(NAME, VALUE)                                                   \
  {                                                                            \
    (uint8_t *)(NAME),   (uint8_t *)(VALUE),   sizeof((NAME)) - 1,             \
    sizeof((VALUE)) - 1, NGHTTP3_NV_FLAG_NONE,                                 \
  }

/*
 * nghttp3_write_frame writes |fr| to |dest|.  This function
 * calculates the payload length and assigns it to fr->hd.length;
 */
void nghttp3_write_frame(nghttp3_buf *dest, nghttp3_frame *fr);

/*
 * nghttp3_write_frame_qpack writes |fr| to |dest|.  |fr| is supposed
 * to be a frame which uses QPACK encoder |qenc|.  |qenc| must be
 * configured so that it does not use dynamic table.  This function
 * calculates the payload length and assigns it to fr->hd.length;
 */
void nghttp3_write_frame_qpack(nghttp3_buf *dest, nghttp3_qpack_encoder *qenc,
                               int64_t stream_id, nghttp3_frame *fr);

/*
 * nghttp3_write_frame_qpack_dyn is similar to
 * nghttp3_write_frame_qpack but it can use dynamic table.  The it
 * will write encoder stream to |ebuf|.
 */
void nghttp3_write_frame_qpack_dyn(nghttp3_buf *dest, nghttp3_buf *ebuf,
                                   nghttp3_qpack_encoder *qenc,
                                   int64_t stream_id, nghttp3_frame *fr);

/*
 * nghttp3_write_frame_data writes DATA frame which has |len| bytes of
 * payload.
 */
void nghttp3_write_frame_data(nghttp3_buf *dest, size_t len);

#endif /* !defined(NGHTTP3_TEST_HELPER) */
