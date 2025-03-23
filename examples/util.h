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
#ifndef UTIL_H
#define UTIL_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif // defined(HAVE_CONFIG_H)

#include <stdint.h>

#ifdef HAVE_ARPA_INET_H
#  include <arpa/inet.h>
#endif // defined(HAVE_ARPA_INET_H)

#ifdef HAVE_NETINET_IN_H
#  include <netinet/in.h>
#endif // defined(HAVE_NETINET_IN_H)

#ifdef HAVE_ENDIAN_H
#  include <endian.h>
#endif // defined(HAVE_ENDIAN_H)x

#ifdef HAVE_SYS_ENDIAN_H
#  include <sys/endian.h>
#endif // defined(HAVE_SYS_ENDIAN_H)

namespace nghttp3 {

#if HAVE_DECL_BE64TOH
#  define nghttp3_ntohl64(N) be64toh(N)
#  define nghttp3_htonl64(N) htobe64(N)
#else // !HAVE_DECL_BE64TOH
#  ifdef WORDS_BIGENDIAN
#    define nghttp3_ntohl64(N) (N)
#    define nghttp3_htonl64(N) (N)
#  else // !defined(WORDS_BIGENDIAN)
#    define nghttp3_bswap64(N)                                                 \
      ((uint64_t)(ntohl((uint32_t)(N))) << 32 | ntohl((uint32_t)((N) >> 32)))
#    define nghttp3_ntohl64(N) nghttp3_bswap64(N)
#    define nghttp3_htonl64(N) nghttp3_bswap64(N)
#  endif // !defined(WORDS_BIGENDIAN)
#endif   // !HAVE_DECL_BE64TOH

} // namespace nghttp3

#endif // !defined(UTIL_H)
