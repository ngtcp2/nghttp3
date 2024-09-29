/*
 * nghttp3
 *
 * Copyright (c) 2019 nghttp3 contributors
 * Copyright (c) 2017 ngtcp2 contributors
 * Copyright (c) 2012 nghttp2 contributors
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
#ifndef NGHTTP3_STR_H
#define NGHTTP3_STR_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* defined(HAVE_CONFIG_H) */

#include <nghttp3/nghttp3.h>

uint8_t *nghttp3_cpymem(uint8_t *dest, const uint8_t *src, size_t n);

void nghttp3_downcase(uint8_t *s, size_t len);

#ifdef __SSE4_2__
/*
 * nghttp3_find_first_of_sse42 returns the pointer to the first
 * position in [|first|, |last|) where its byte value is included in
 * |ranges| of length |rangeslen|, otherwise returns |last|.  |ranges|
 * must be a sequence of ranges without delimiters, and the range is
 * inclusive (e.g., \x01\x0a).  |rangeslen| must be equal or less than
 * 16.  While |rangeslen| can be less than 16, the memory region
 * [|ranges|, |ranges| + 16) must be accessible.  The distance between
 * |first| and |last| must be divisible by 16.  This function uses
 * SSE4.2 intrinsics.
 */
const uint8_t *nghttp3_find_first_of_sse42(const uint8_t *first,
                                           const uint8_t *last,
                                           const uint8_t *ranges,
                                           size_t rangeslen);
#endif /* __SSE4_2__ */

#endif /* !defined(NGHTTP3_STR_H) */
