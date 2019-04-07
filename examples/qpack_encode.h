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
#ifndef QPACK_ENCODE_H
#define QPACK_ENCODE_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <nghttp3/nghttp3.h>

#include <string>

namespace nghttp3 {

class Encoder {
public:
  Encoder(size_t max_dtable_size, size_t max_blocked, bool immediate_ack);
  ~Encoder();

  int init();
  int encode(nghttp3_buf *pbuf, nghttp3_buf *rbuf, nghttp3_buf *ebuf,
             int64_t stream_id, const nghttp3_nv *nva, size_t len);

private:
  const nghttp3_mem *mem_;
  nghttp3_qpack_encoder *enc_;
  size_t max_dtable_size_;
  size_t max_blocked_;
  bool immediate_ack_;
};

int encode(const std::string_view &outfile, const std::string_view &infile);

} // namespace nghttp3

#endif // QPACK_ENCODE_H
