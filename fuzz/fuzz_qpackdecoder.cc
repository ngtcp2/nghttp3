#include <arpa/inet.h>

#include <cassert>
#include <cstring>
#include <vector>
#include <queue>
#include <functional>
#include <utility>
#include <memory>
#include <string>
#include <span>
#include <ranges>
#include <expected>

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

enum class Error {
  QPACK,
  BLOCKED,
};

struct Request {
  Request(int64_t stream_id, std::span<const uint8_t> data);
  ~Request();

  std::expected<void, Error> init(const nghttp3_mem &mem);

  std::vector<uint8_t> data_store;
  std::span<const uint8_t> data;
  nghttp3_qpack_stream_context *sctx;
  int64_t stream_id;
};

namespace std {
template <> struct greater<std::shared_ptr<Request>> {
  bool operator()(const std::shared_ptr<Request> &lhs,
                  const std::shared_ptr<Request> &rhs) const {
    return nghttp3_qpack_stream_context_get_ricnt2(lhs->sctx) >
           nghttp3_qpack_stream_context_get_ricnt2(rhs->sctx);
  }
};
} // namespace std

using Headers = std::vector<std::pair<std::string, std::string>>;

class Decoder {
public:
  Decoder(size_t max_dtable_size, size_t max_blocked, const nghttp3_mem &mem);
  ~Decoder();

  std::expected<void, Error> init();
  std::expected<void, Error> read_encoder(std::span<const uint8_t> data);
  std::expected<Headers, Error> read_request(std::span<const uint8_t> data,
                                             int64_t stream_id);
  std::expected<Headers, Error> read_request(Request &req);
  std::expected<Headers, Error> process_blocked();
  size_t get_num_blocked() const;

private:
  const nghttp3_mem &mem_;
  nghttp3_qpack_decoder *dec_;
  std::priority_queue<std::shared_ptr<Request>,
                      std::vector<std::shared_ptr<Request>>,
                      std::greater<std::shared_ptr<Request>>>
    blocked_reqs_;
  size_t max_dtable_size_;
  size_t max_blocked_;
};

Request::Request(int64_t stream_id, std::span<const uint8_t> data)
  : data_store{std::ranges::to<std::vector>(data)},
    data{data_store},
    sctx(nullptr),
    stream_id(stream_id) {}

std::expected<void, Error> Request::init(const nghttp3_mem &mem) {
  auto rv = nghttp3_qpack_stream_context_new(&sctx, stream_id, &mem);
  if (rv != 0) {
    return std::unexpected{Error::QPACK};
  }

  return {};
}

Request::~Request() { nghttp3_qpack_stream_context_del(sctx); }

Decoder::Decoder(size_t max_dtable_size, size_t max_blocked,
                 const nghttp3_mem &mem)
  : mem_(mem),
    dec_(nullptr),
    max_dtable_size_(max_dtable_size),
    max_blocked_(max_blocked) {}

Decoder::~Decoder() { nghttp3_qpack_decoder_del(dec_); }

std::expected<void, Error> Decoder::init() {
  if (auto rv =
        nghttp3_qpack_decoder_new(&dec_, max_dtable_size_, max_blocked_, &mem_);
      rv != 0) {
    return std::unexpected{Error::QPACK};
  }

  nghttp3_qpack_decoder_set_max_dtable_capacity(dec_, max_dtable_size_);

  return {};
}

std::expected<void, Error>
Decoder::read_encoder(std::span<const uint8_t> data) {
  auto nread =
    nghttp3_qpack_decoder_read_encoder(dec_, data.data(), data.size());
  if (nread < 0) {
    return std::unexpected{Error::QPACK};
  }

  assert(static_cast<size_t>(nread) == data.size());

  return {};
}

std::expected<Headers, Error>
Decoder::read_request(std::span<const uint8_t> data, int64_t stream_id) {
  auto req = std::make_shared<Request>(stream_id, data);
  if (auto rv = req->init(mem_); !rv) {
    return std::unexpected{rv.error()};
  }

  auto maybe_headers = read_request(*req);
  if (!maybe_headers && maybe_headers.error() == Error::BLOCKED) {
    if (blocked_reqs_.size() >= max_blocked_) {
      return std::unexpected{Error::QPACK};
    }

    blocked_reqs_.emplace(std::move(req));
  }

  return maybe_headers;
}

std::expected<Headers, Error> Decoder::read_request(Request &req) {
  nghttp3_qpack_nv nv;
  uint8_t flags;
  Headers headers;

  for (;;) {
    auto nread = nghttp3_qpack_decoder_read_request(
      dec_, req.sctx, &nv, &flags, req.data.data(), req.data.size(), 1);
    if (nread < 0) {
      return std::unexpected{Error::QPACK};
    }

    req.data = req.data.subspan(nread);

    if (flags & NGHTTP3_QPACK_DECODE_FLAG_FINAL) {
      break;
    }

    if (flags & NGHTTP3_QPACK_DECODE_FLAG_BLOCKED) {
      return std::unexpected{Error::BLOCKED};
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

  return headers;
}

std::expected<Headers, Error> Decoder::process_blocked() {
  if (blocked_reqs_.empty()) {
    return std::unexpected{Error::BLOCKED};
  }

  auto &top = blocked_reqs_.top();
  if (nghttp3_qpack_stream_context_get_ricnt2(top->sctx) >
      nghttp3_qpack_decoder_get_icnt(dec_)) {
    return std::unexpected{Error::BLOCKED};
  }

  auto req = top;
  blocked_reqs_.pop();

  return read_request(*req);
}

namespace {
void *fuzzed_malloc(size_t size, void *user_data) {
  auto fuzzed_data_provider = static_cast<FuzzedDataProvider *>(user_data);

  return fuzzed_data_provider->ConsumeBool() ? nullptr : malloc(size);
}
} // namespace

namespace {
void *fuzzed_calloc(size_t nmemb, size_t size, void *user_data) {
  auto fuzzed_data_provider = static_cast<FuzzedDataProvider *>(user_data);

  return fuzzed_data_provider->ConsumeBool() ? nullptr : calloc(nmemb, size);
}
} // namespace

namespace {
void *fuzzed_realloc(void *ptr, size_t size, void *user_data) {
  auto fuzzed_data_provider = static_cast<FuzzedDataProvider *>(user_data);

  return fuzzed_data_provider->ConsumeBool() ? nullptr : realloc(ptr, size);
}
} // namespace

std::expected<void, Error> decode(const uint8_t *data, size_t datalen) {
  FuzzedDataProvider fuzzed_data_provider(data, datalen);

  auto mem = *nghttp3_mem_default();
  mem.user_data = &fuzzed_data_provider;
  mem.malloc = fuzzed_malloc;
  mem.calloc = fuzzed_calloc;
  mem.realloc = fuzzed_realloc;

  auto max_dtable_size =
    fuzzed_data_provider.ConsumeIntegralInRange<size_t>(0, NGHTTP3_MAX_VARINT);
  auto max_blocked =
    fuzzed_data_provider.ConsumeIntegralInRange<size_t>(0, NGHTTP3_MAX_VARINT);

  auto dec = Decoder(max_dtable_size, max_blocked, mem);
  if (auto rv = dec.init(); !rv) {
    return rv;
  }

  const auto encoder_stream_id =
    fuzzed_data_provider.ConsumeIntegralInRange<int64_t>(0, NGHTTP3_MAX_VARINT);

  for (; fuzzed_data_provider.remaining_bytes();) {
    auto stream_id = fuzzed_data_provider.ConsumeIntegralInRange<int64_t>(
      0, NGHTTP3_MAX_VARINT);
    auto chunk_size = fuzzed_data_provider.ConsumeIntegral<size_t>();
    auto chunk = fuzzed_data_provider.ConsumeBytes<uint8_t>(chunk_size);

    if (stream_id == encoder_stream_id) {
      if (auto rv = dec.read_encoder(chunk); !rv) {
        return rv;
      }

      for (;;) {
        auto maybe_headers = dec.process_blocked();
        if (!maybe_headers) {
          auto err = maybe_headers.error();
          if (err == Error::BLOCKED) {
            break;
          }

          return std::unexpected{err};
        }
      }

      continue;
    }

    auto maybe_headers = dec.read_request(chunk, stream_id);
    if (!maybe_headers && maybe_headers.error() != Error::BLOCKED) {
      return std::unexpected{maybe_headers.error()};
    }
  }

  return {};
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  (void)decode(data, size);
  return 0;
}
