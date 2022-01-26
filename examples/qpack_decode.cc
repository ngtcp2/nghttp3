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
#include "qpack_decode.h"

#include <arpa/inet.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <cassert>
#include <cstring>
#include <cerrno>
#include <iostream>
#include <fstream>

#include "qpack.h"
#include "template.h"
#include "util.h"

namespace nghttp3 {

extern Config config;

Request::Request(int64_t stream_id, const nghttp3_buf *buf)
    : buf(*buf), stream_id(stream_id) {
  auto mem = nghttp3_mem_default();
  nghttp3_qpack_stream_context_new(&sctx, stream_id, mem);
}

Request::~Request() { nghttp3_qpack_stream_context_del(sctx); }

Decoder::Decoder(size_t max_dtable_size, size_t max_blocked)
    : mem_(nghttp3_mem_default()),
      dec_(nullptr),
      max_dtable_size_(max_dtable_size),
      max_blocked_(max_blocked) {}

Decoder::~Decoder() { nghttp3_qpack_decoder_del(dec_); }

int Decoder::init() {
  if (auto rv = nghttp3_qpack_decoder_new(&dec_, max_dtable_size_, max_blocked_,
                                          mem_);
      rv != 0) {
    std::cerr << "nghttp3_qpack_decoder_new: " << nghttp3_strerror(rv)
              << std::endl;
    return -1;
  }

  if (auto rv =
          nghttp3_qpack_decoder_set_max_dtable_capacity(dec_, max_dtable_size_);
      rv != 0) {
    std::cerr << "nghttp3_qpack_decoder_set_max_dtable_capacity: "
              << nghttp3_strerror(rv) << std::endl;
    return -1;
  }

  return 0;
}

int Decoder::read_encoder(nghttp3_buf *buf) {
  auto nread =
      nghttp3_qpack_decoder_read_encoder(dec_, buf->pos, nghttp3_buf_len(buf));
  if (nread < 0) {
    std::cerr << "nghttp3_qpack_decoder_read_encoder: "
              << nghttp3_strerror(nread) << std::endl;
    return -1;
  }

  assert(static_cast<size_t>(nread) == nghttp3_buf_len(buf));

  return 0;
}

std::tuple<Headers, int> Decoder::read_request(nghttp3_buf *buf,
                                               int64_t stream_id) {
  auto req = std::make_shared<Request>(stream_id, buf);

  auto [headers, rv] = read_request(*req);
  if (rv == -1) {
    return {Headers{}, -1};
  }
  if (rv == 1) {
    if (blocked_reqs_.size() >= max_blocked_) {
      std::cerr << "Too many blocked streams: max_blocked=" << max_blocked_
                << std::endl;
      return {Headers{}, -1};
    }
    blocked_reqs_.emplace(std::move(req));
    return {Headers{}, 1};
  }
  return {headers, 0};
}

std::tuple<Headers, int> Decoder::read_request(Request &req) {
  nghttp3_qpack_nv nv;
  uint8_t flags;
  Headers headers;

  for (;;) {
    auto nread = nghttp3_qpack_decoder_read_request(
        dec_, req.sctx, &nv, &flags, req.buf.pos, nghttp3_buf_len(&req.buf), 1);
    if (nread < 0) {
      std::cerr << "nghttp3_qpack_decoder_read_request: "
                << nghttp3_strerror(nread) << std::endl;
      return {Headers{}, -1};
    }

    req.buf.pos += nread;

    if (flags & NGHTTP3_QPACK_DECODE_FLAG_FINAL) {
      break;
    }
    if (flags & NGHTTP3_QPACK_DECODE_FLAG_BLOCKED) {
      return {Headers{}, 1};
    }
    if (flags & NGHTTP3_QPACK_DECODE_FLAG_EMIT) {
      auto name = nghttp3_rcbuf_get_buf(nv.name);
      auto value = nghttp3_rcbuf_get_buf(nv.value);
      headers.emplace_back(std::string{name.base, name.base + name.len},
                           std::string{value.base, value.base + value.len});
      nghttp3_rcbuf_decref(nv.name);
      nghttp3_rcbuf_decref(nv.value);
    }
  }

  return {headers, 0};
}

std::tuple<int64_t, Headers, int> Decoder::process_blocked() {
  if (!blocked_reqs_.empty()) {
    auto &top = blocked_reqs_.top();
    if (nghttp3_qpack_stream_context_get_ricnt(top->sctx) >
        nghttp3_qpack_decoder_get_icnt(dec_)) {
      return {-1, {}, 0};
    }

    auto req = top;
    blocked_reqs_.pop();

    auto [headers, rv] = read_request(*req);
    if (rv < 0) {
      return {-1, {}, -1};
    }
    assert(rv == 0);

    return {req->stream_id, headers, 0};
  }
  return {-1, {}, 0};
}

size_t Decoder::get_num_blocked() const { return blocked_reqs_.size(); }

namespace {
void write_header(
    std::ostream &out,
    const std::vector<std::pair<std::string, std::string>> &headers) {
  for (auto &nv : headers) {
    out.write(nv.first.c_str(), nv.first.size());
    out.put('\t');
    out.write(nv.second.c_str(), nv.second.size());
    out.put('\n');
  }
  out.put('\n');
}
} // namespace

int decode(const std::string_view &outfile, const std::string_view &infile) {
  auto fd = open(infile.data(), O_RDONLY);
  if (fd == -1) {
    std::cerr << "Could not open " << infile << ": " << strerror(errno)
              << std::endl;
    return -1;
  }

  auto fd_closer = defer(close, fd);

  struct stat st;
  if (fstat(fd, &st) == -1) {
    std::cerr << "fstat: " << strerror(errno) << std::endl;
    return -1;
  }

  auto out = std::ofstream(outfile.data(), std::ios::trunc | std::ios::binary);
  if (!out) {
    std::cerr << "Could not open file " << outfile << ": " << strerror(errno)
              << std::endl;
    return -1;
  }

  auto in = reinterpret_cast<uint8_t *>(
      mmap(nullptr, st.st_size, PROT_READ, MAP_SHARED, fd, 0));
  if (in == MAP_FAILED) {
    std::cerr << "mmap: " << strerror(errno) << std::endl;
    return -1;
  }

  auto unmapper = defer(munmap, in, st.st_size);

  auto dec = Decoder(config.max_dtable_size, config.max_blocked);
  if (auto rv = dec.init(); rv != 0) {
    return rv;
  }

  for (auto p = in, end = in + st.st_size; p != end;) {
    int64_t stream_id;
    uint32_t size;

    if (static_cast<size_t>(end - p) < sizeof(stream_id) + sizeof(size)) {
      std::cerr << "Could not read stream ID and size" << std::endl;
      return -1;
    }

    memcpy(&stream_id, p, sizeof(stream_id));
    stream_id = nghttp3_ntohl64(stream_id);
    p += sizeof(stream_id);

    memcpy(&size, p, sizeof(size));
    size = ntohl(size);
    p += sizeof(size);

    if ((size_t)(end - p) < size) {
      std::cerr << "Insufficient input: require " << size << " but "
                << (end - p) << " is available" << std::endl;
      return -1;
    }

    nghttp3_buf buf;
    buf.begin = buf.pos = p;
    buf.end = buf.last = p + size;

    p += size;

    if (stream_id == 0) {
      if (auto rv = dec.read_encoder(&buf); rv != 0) {
        return rv;
      }

      for (;;) {
        auto [stream_id, headers, rv] = dec.process_blocked();
        if (rv != 0) {
          return rv;
        }

        if (stream_id == -1) {
          break;
        }

        write_header(out, headers);
      }

      continue;
    }

    auto [headers, rv] = dec.read_request(&buf, stream_id);
    if (rv == -1) {
      return rv;
    }
    if (rv == 1) {
      // Stream blocked
      continue;
    }

    write_header(out, headers);
  }

  if (auto n = dec.get_num_blocked(); n) {
    std::cerr << "Still " << n << " stream(s) blocked" << std::endl;
    return -1;
  }

  return 0;
}

} // namespace nghttp3
