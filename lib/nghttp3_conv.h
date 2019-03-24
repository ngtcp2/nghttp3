/*
 * nghttp3
 *
 * Copyright (c) 2019 nghttp3 contributors
 * Copyright (c) 2017 ngtcp2 contributors
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
#ifndef NGHTTP3_CONV_H
#define NGHTTP3_CONV_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_ARPA_INET_H
#  include <arpa/inet.h>
#endif /* HAVE_ARPA_INET_H */

#include <nghttp3/nghttp3.h>

#ifdef WORDS_BIGENDIAN
#  define bswap64(N) (N)
#else /* !WORDS_BIGENDIAN */
#  define bswap64(N)                                                           \
    ((uint64_t)(ntohl((uint32_t)(N))) << 32 | ntohl((uint32_t)((N) >> 32)))
#endif /* !WORDS_BIGENDIAN */

/*
 * nghttp3_get_varint reads variable-length integer from |p|, and
 * returns it in host byte order.  The number of bytes read is stored
 * in |*plen|.
 */
int64_t nghttp3_get_varint(size_t *plen, const uint8_t *p);

/*
 * nghttp3_get_varint_fb reads first byte of encoded variable-length
 * integer from |p|.
 */
int64_t nghttp3_get_varint_fb(const uint8_t *p);

/*
 * nghttp3_get_varint_len returns the required number of bytes to read
 * variable-length integer starting at |p|.
 */
size_t nghttp3_get_varint_len(const uint8_t *p);

/*
 * nghttp3_put_uint64be writes |n| in host byte order in |p| in
 * network byte order.  It returns the one beyond of the last written
 * position.
 */
uint8_t *nghttp3_put_uint64be(uint8_t *p, uint64_t n);

/*
 * nghttp3_put_uint48be writes |n| in host byte order in |p| in
 * network byte order.  It writes only least significant 48 bits.  It
 * returns the one beyond of the last written position.
 */
uint8_t *nghttp3_put_uint48be(uint8_t *p, uint64_t n);

/*
 * nghttp3_put_uint32be writes |n| in host byte order in |p| in
 * network byte order.  It returns the one beyond of the last written
 * position.
 */
uint8_t *nghttp3_put_uint32be(uint8_t *p, uint32_t n);

/*
 * nghttp3_put_uint24be writes |n| in host byte order in |p| in
 * network byte order.  It writes only least significant 24 bits.  It
 * returns the one beyond of the last written position.
 */
uint8_t *nghttp3_put_uint24be(uint8_t *p, uint32_t n);

/*
 * nghttp3_put_uint16be writes |n| in host byte order in |p| in
 * network byte order.  It returns the one beyond of the last written
 * position.
 */
uint8_t *nghttp3_put_uint16be(uint8_t *p, uint16_t n);

/*
 * nghttp3_put_varint writes |n| in |p| using variable-length integer
 * encoding.  It returns the one beyond of the last written position.
 */
uint8_t *nghttp3_put_varint(uint8_t *p, int64_t n);

/*
 * nghttp3_put_varint_len returns the required number of bytes to
 * encode |n|.
 */
size_t nghttp3_put_varint_len(int64_t n);

/*
 * nghttp3_ord_stream_id returns the ordinal number of |stream_id|.
 */
uint64_t nghttp3_ord_stream_id(int64_t stream_id);

#endif /* NGHTTP3_CONV_H */
