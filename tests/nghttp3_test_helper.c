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

#include <assert.h>

void nghttp3_write_frame(nghttp3_buf *dest, nghttp3_frame *fr) {
  size_t payloadlen;
  int rv = 0;

  switch (fr->hd.type) {
  case NGHTTP3_FRAME_SETTINGS:
    nghttp3_frame_write_settings_len(&payloadlen, &fr->settings);
    fr->hd.length = (int64_t)payloadlen;
    rv = nghttp3_frame_write_settings(dest, &fr->settings);
    break;
  case NGHTTP3_FRAME_PRIORITY:
    nghttp3_frame_write_priority_len(&payloadlen, &fr->priority);
    fr->hd.length = (int64_t)payloadlen;
    rv = nghttp3_frame_write_priority(dest, &fr->priority);
    break;
  }

  assert(0 == rv);
}
