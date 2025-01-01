#include <arpa/inet.h>

#include <cassert>
#include <cstring>
#include <vector>
#include <queue>
#include <functional>
#include <utility>
#include <memory>
#include <string>

#include <fuzzer/FuzzedDataProvider.h>

#include <nghttp3/nghttp3.h>

#ifdef __cplusplus
extern "C" {
#endif // defined(__cplusplus)

#include "nghttp3_macro.h"

#ifdef __cplusplus
}
#endif // defined(__cplusplus)

#define nghttp3_ntohl64(N) be64toh(N)

struct Request {
  Request(int64_t stream_id, const nghttp3_buf *buf);
  ~Request();

  nghttp3_buf buf;
  nghttp3_qpack_stream_context *sctx;
  int64_t stream_id;
};

namespace std {
template <> struct greater<std::shared_ptr<Request>> {
  bool operator()(const std::shared_ptr<Request> &lhs,
                  const std::shared_ptr<Request> &rhs) const {
    return nghttp3_qpack_stream_context_get_ricnt(lhs->sctx) >
           nghttp3_qpack_stream_context_get_ricnt(rhs->sctx);
  }
};
} // namespace std

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
  if (auto rv =
        nghttp3_qpack_decoder_new(&dec_, max_dtable_size_, max_blocked_, mem_);
      rv != 0) {
    return -1;
  }

  nghttp3_qpack_decoder_set_max_dtable_capacity(dec_, max_dtable_size_);

  return 0;
}

int Decoder::read_encoder(nghttp3_buf *buf) {
  auto nread =
    nghttp3_qpack_decoder_read_encoder(dec_, buf->pos, nghttp3_buf_len(buf));
  if (nread < 0) {
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

int decode(const uint8_t *data, size_t datalen) {
  FuzzedDataProvider fuzzed_data_provider(data, datalen);

  auto max_dtable_size =
    fuzzed_data_provider.ConsumeIntegralInRange<size_t>(0, NGHTTP3_MAX_VARINT);
  auto max_blocked =
    fuzzed_data_provider.ConsumeIntegralInRange<size_t>(0, NGHTTP3_MAX_VARINT);

  auto dec = Decoder(max_dtable_size, max_blocked);
  if (auto rv = dec.init(); rv != 0) {
    return rv;
  }

  const auto encoder_stream_id =
    fuzzed_data_provider.ConsumeIntegralInRange<int64_t>(0, NGHTTP3_MAX_VARINT);

  for (; fuzzed_data_provider.remaining_bytes();) {
    auto stream_id = fuzzed_data_provider.ConsumeIntegralInRange<int64_t>(
      0, NGHTTP3_MAX_VARINT);
    auto chunk_size = fuzzed_data_provider.ConsumeIntegral<size_t>();
    auto chunk = fuzzed_data_provider.ConsumeBytes<uint8_t>(chunk_size);

    nghttp3_buf buf{
      .begin = chunk.data(),
      .end = chunk.data() + chunk.size(),
    };

    if (stream_id == encoder_stream_id) {
      if (auto rv = dec.read_encoder(&buf); rv != 0) {
        return rv;
      }

      for (;;) {
        auto [stream_id, _, rv] = dec.process_blocked();
        if (rv != 0) {
          return rv;
        }

        if (stream_id == -1) {
          break;
        }
      }

      continue;
    }

    auto [_, rv] = dec.read_request(&buf, stream_id);
    if (rv == -1) {
      return rv;
    }
  }

  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  decode(data, size);
  return 0;
}
