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
#include "nghttp3_err.h"

const char *nghttp3_strerror(int liberr) {
  switch (liberr) {
  case NGHTTP3_ERR_INVALID_ARGUMENT:
    return "ERR_INVALID_ARGUMENT";
  case NGHTTP3_ERR_QPACK_FATAL:
    return "ERR_QPACK_FATAL";
  case NGHTTP3_ERR_QPACK_DECOMPRESSION_FAILED:
    return "ERR_QPACK_DECOMPRESSION_FAILED";
  case NGHTTP3_ERR_QPACK_ENCODER_STREAM_ERROR:
    return "ERR_QPACK_ENCODER_STREAM_ERROR";
  case NGHTTP3_ERR_QPACK_DECODER_STREAM_ERROR:
    return "ERR_QPACK_DECODER_STREAM_ERROR";
  case NGHTTP3_ERR_QPACK_HEADER_TOO_LARGE:
    return "ERR_QPACK_HEADER_TOO_LARGE";
  case NGHTTP3_ERR_NOMEM:
    return "ERR_NOMEM";
  default:
    return "(unknown)";
  }
}

int nghttp3_err_malformed_frame(int64_t type) {
  if (type > 0xfe) {
    return NGHTTP3_ERR_HTTP_MALFORMED_FRAME - 0xff;
  }
  return NGHTTP3_ERR_HTTP_MALFORMED_FRAME - (int)type;
}
