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
#include "qpack_encode.h"

#include <arpa/inet.h>

#include <cerrno>
#include <cstring>
#include <cassert>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <array>
#include <iomanip>
#include <vector>

#include "qpack.h"
#include "template.h"
#include "util.h"

namespace nghttp3 {

extern Config config;

Encoder::Encoder(size_t max_dtable_size, size_t max_blocked, bool immediate_ack)
  : mem_(nghttp3_mem_default()),
    enc_(nullptr),
    max_dtable_size_(max_dtable_size),
    max_blocked_(max_blocked),
    immediate_ack_(immediate_ack) {}

Encoder::~Encoder() { nghttp3_qpack_encoder_del(enc_); }

int Encoder::init() {
  int rv;

  rv = nghttp3_qpack_encoder_new(&enc_, max_dtable_size_, mem_);
  if (rv != 0) {
    std::cerr << "nghttp3_qpack_encoder_new: " << nghttp3_strerror(rv)
              << std::endl;
    return -1;
  }

  nghttp3_qpack_encoder_set_max_dtable_capacity(enc_, max_dtable_size_);
  nghttp3_qpack_encoder_set_max_blocked_streams(enc_, max_blocked_);

  return 0;
}

int Encoder::encode(nghttp3_buf *pbuf, nghttp3_buf *rbuf, nghttp3_buf *ebuf,
                    int64_t stream_id, const nghttp3_nv *nva, size_t len) {
  auto rv =
    nghttp3_qpack_encoder_encode(enc_, pbuf, rbuf, ebuf, stream_id, nva, len);
  if (rv != 0) {
    std::cerr << "nghttp3_qpack_encoder_encode: " << nghttp3_strerror(rv)
              << std::endl;
    return -1;
  }
  if (immediate_ack_) {
    nghttp3_qpack_encoder_ack_everything(enc_);
  }
  return 0;
}

namespace {
void write_encoder_stream(std::ostream &out, nghttp3_buf *ebuf) {
  uint64_t stream_id = 0;
  out.write(reinterpret_cast<char *>(&stream_id), sizeof(stream_id));
  uint32_t size = htonl(nghttp3_buf_len(ebuf));
  out.write(reinterpret_cast<char *>(&size), sizeof(size));
  out.write(reinterpret_cast<char *>(ebuf->pos), nghttp3_buf_len(ebuf));
}
} // namespace

namespace {
void write_request_stream(std::ostream &out, int64_t stream_id,
                          nghttp3_buf *pbuf, nghttp3_buf *rbuf) {
  stream_id = nghttp3_htonl64(stream_id);
  out.write(reinterpret_cast<char *>(&stream_id), sizeof(stream_id));
  uint32_t size = htonl(nghttp3_buf_len(pbuf) + nghttp3_buf_len(rbuf));
  out.write(reinterpret_cast<char *>(&size), sizeof(size));
  out.write(reinterpret_cast<char *>(pbuf->pos), nghttp3_buf_len(pbuf));
  out.write(reinterpret_cast<char *>(rbuf->pos), nghttp3_buf_len(rbuf));
}
} // namespace

int encode(const std::string_view &outfile, const std::string_view &infile) {
  auto in = std::ifstream(infile.data(), std::ios::binary);

  if (!in) {
    std::cerr << "Could not open file " << infile << ": " << strerror(errno)
              << std::endl;
    return -1;
  }

  auto out = std::ofstream(outfile.data(), std::ios::trunc | std::ios::binary);
  if (!out) {
    std::cerr << "Could not open file " << outfile << ": " << strerror(errno)
              << std::endl;
    return -1;
  }

  auto enc =
    Encoder(config.max_dtable_size, config.max_blocked, config.immediate_ack);
  if (enc.init() != 0) {
    return -1;
  }

  nghttp3_buf pbuf, rbuf, ebuf;
  nghttp3_buf_init(&pbuf);
  nghttp3_buf_init(&rbuf);
  nghttp3_buf_init(&ebuf);

  auto mem = nghttp3_mem_default();
  auto pbufd = defer(nghttp3_buf_free, &pbuf, mem);
  auto rbufd = defer(nghttp3_buf_free, &rbuf, mem);
  auto ebufd = defer(nghttp3_buf_free, &ebuf, mem);

  int64_t stream_id = 1;
  std::array<std::string, 1024> sarray;

  size_t srclen = 0;
  size_t enclen = 0;
  size_t rslen = 0;
  size_t eslen = 0;

  for (; in;) {
    auto nva = std::vector<nghttp3_nv>();
    for (std::string line; std::getline(in, line);) {
      if (line == "") {
        break;
      }

      if (sarray.size() == nva.size()) {
        std::cerr << "Too many headers: " << nva.size() << std::endl;
        return -1;
      }

      sarray[nva.size()] = line;
      const auto &s = sarray[nva.size()];

      auto d = s.find('\t');
      if (d == std::string_view::npos) {
        std::cerr << "Could not find TAB in " << s << std::endl;
        return -1;
      }
      auto name = std::string_view(s.c_str(), d);
      auto value = std::string_view(s.c_str() + d + 1, s.size() - d - 1);
      value.remove_prefix(std::min(value.find_first_not_of(" "), value.size()));

      srclen += name.size() + value.size();

      nva.emplace_back(nghttp3_nv{
        const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(name.data())),
        const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(value.data())),
        name.size(), value.size()});
    }

    if (nva.empty()) {
      break;
    }

    if (auto rv =
          enc.encode(&pbuf, &rbuf, &ebuf, stream_id, nva.data(), nva.size());
        rv != 0) {
      return -1;
    }

    enclen +=
      nghttp3_buf_len(&pbuf) + nghttp3_buf_len(&rbuf) + nghttp3_buf_len(&ebuf);

    if (nghttp3_buf_len(&ebuf)) {
      write_encoder_stream(out, &ebuf);
    }
    write_request_stream(out, stream_id, &pbuf, &rbuf);

    rslen += nghttp3_buf_len(&pbuf) + nghttp3_buf_len(&rbuf);
    eslen += nghttp3_buf_len(&ebuf);

    nghttp3_buf_reset(&pbuf);
    nghttp3_buf_reset(&rbuf);
    nghttp3_buf_reset(&ebuf);

    ++stream_id;
  }

  if (srclen == 0) {
    std::cerr << "No header field processed" << std::endl;
  } else {
    std::cerr << srclen << " -> " << enclen << " (r:" << rslen
              << " + e:" << eslen << ") " << std::fixed << std::setprecision(2)
              << (1. - (static_cast<double>(enclen) / srclen)) * 100
              << "% compressed" << std::endl;
  }
  return 0;
}

} // namespace nghttp3
