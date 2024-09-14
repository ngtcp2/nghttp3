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
#ifndef QPACK_DECODE_H
#define QPACK_DECODE_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif // defined(HAVE_CONFIG_H)

#include <nghttp3/nghttp3.h>

#include <vector>
#include <queue>
#include <functional>
#include <utility>
#include <memory>
#include <string>

namespace nghttp3 {
struct Request {
  Request(int64_t stream_id, const nghttp3_buf *buf);
  ~Request();

  nghttp3_buf buf;
  nghttp3_qpack_stream_context *sctx;
  int64_t stream_id;
};
} // namespace nghttp3

namespace std {
template <> struct greater<std::shared_ptr<nghttp3::Request>> {
  bool operator()(const std::shared_ptr<nghttp3::Request> &lhs,
                  const std::shared_ptr<nghttp3::Request> &rhs) const {
    return nghttp3_qpack_stream_context_get_ricnt(lhs->sctx) >
           nghttp3_qpack_stream_context_get_ricnt(rhs->sctx);
  }
};
} // namespace std

namespace nghttp3 {

using Headers = std::vector<std::pair<std::string, std::string>>;

class Decoder {
public:
  Decoder(size_t max_dtable_size, size_t max_blocked);
  ~Decoder();

  int init();
  int read_encoder(nghttp3_buf *buf);
  std::tuple<Headers, int> read_request(nghttp3_buf *buf, int64_t stream_id);
  std::tuple<Headers, int> read_request(Request &req);
  std::tuple<int64_t, Headers, int> process_blocked();
  size_t get_num_blocked() const;

private:
  const nghttp3_mem *mem_;
  nghttp3_qpack_decoder *dec_;
  std::priority_queue<std::shared_ptr<Request>,
                      std::vector<std::shared_ptr<Request>>,
                      std::greater<std::shared_ptr<Request>>>
    blocked_reqs_;
  size_t max_dtable_size_;
  size_t max_blocked_;
};

int decode(const std::string_view &outfile, const std::string_view &infile);

} // namespace nghttp3

#endif // !defined(QPACK_DECODE_H)
