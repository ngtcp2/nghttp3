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
#include "util.h"

#include <arpa/inet.h>

namespace nghttp3 {

#ifdef WORDS_BIGENDIAN
uint64_t ntohl64(uint64_t n) { return n; }
uint64_t htonl64(uint64_t n) { return n; }
#else  // !WORDS_BIGENDIAN
namespace {
uint64_t bswap64(uint64_t n) {
  return (static_cast<uint64_t>(ntohl(static_cast<uint32_t>(n))) << 32 |
          ntohl(static_cast<uint32_t>((n) >> 32)));
}
} // namespace

uint64_t ntohl64(uint64_t n) { return bswap64(n); }
uint64_t htonl64(uint64_t n) { return bswap64(n); }
#endif // !WORDS_BIGENDIAN

} // namespace nghttp3
