#include <array>

#include <fuzzer/FuzzedDataProvider.h>

#include <nghttp3/nghttp3.h>

#ifdef __cplusplus
extern "C" {
#endif // defined(__cplusplus)

#include "nghttp3_macro.h"
#include "nghttp3_stream.h"

#ifdef __cplusplus
}
#endif // defined(__cplusplus)

static int acked_stream_data(nghttp3_conn *conn, int64_t stream_id,
                             uint64_t datalen, void *conn_user_data,
                             void *stream_user_data) {
  auto fuzzed_data_provider = static_cast<FuzzedDataProvider *>(conn_user_data);

  return fuzzed_data_provider->ConsumeBool() ? NGHTTP3_ERR_CALLBACK_FAILURE : 0;
}

static int stream_close(nghttp3_conn *conn, int64_t stream_id,
                        uint64_t app_error_code, void *conn_user_data,
                        void *stream_user_data) {
  auto fuzzed_data_provider = static_cast<FuzzedDataProvider *>(conn_user_data);

  return fuzzed_data_provider->ConsumeBool() ? NGHTTP3_ERR_CALLBACK_FAILURE : 0;
}

static int recv_data(nghttp3_conn *conn, int64_t stream_id, const uint8_t *data,
                     size_t datalen, void *conn_user_data,
                     void *stream_user_data) {
  auto fuzzed_data_provider = static_cast<FuzzedDataProvider *>(conn_user_data);

  return fuzzed_data_provider->ConsumeBool() ? NGHTTP3_ERR_CALLBACK_FAILURE : 0;
}

static int deferred_consume(nghttp3_conn *conn, int64_t stream_id,
                            size_t consumed, void *conn_user_data,
                            void *stream_user_data) {
  auto fuzzed_data_provider = static_cast<FuzzedDataProvider *>(conn_user_data);

  return fuzzed_data_provider->ConsumeBool() ? NGHTTP3_ERR_CALLBACK_FAILURE : 0;
}

static int begin_headers(nghttp3_conn *conn, int64_t stream_id,
                         void *conn_user_data, void *stream_user_data) {
  auto fuzzed_data_provider = static_cast<FuzzedDataProvider *>(conn_user_data);

  return fuzzed_data_provider->ConsumeBool() ? NGHTTP3_ERR_CALLBACK_FAILURE : 0;
}

static int recv_header(nghttp3_conn *conn, int64_t stream_id, int32_t token,
                       nghttp3_rcbuf *name, nghttp3_rcbuf *value, uint8_t flags,
                       void *conn_user_data, void *stream_user_data) {
  auto fuzzed_data_provider = static_cast<FuzzedDataProvider *>(conn_user_data);

  return fuzzed_data_provider->ConsumeBool() ? NGHTTP3_ERR_CALLBACK_FAILURE : 0;
}

static int end_headers(nghttp3_conn *conn, int64_t stream_id, int fin,
                       void *conn_user_data, void *stream_user_data) {
  auto fuzzed_data_provider = static_cast<FuzzedDataProvider *>(conn_user_data);

  return fuzzed_data_provider->ConsumeBool() ? NGHTTP3_ERR_CALLBACK_FAILURE : 0;
}

static int begin_trailers(nghttp3_conn *conn, int64_t stream_id,
                          void *conn_user_data, void *stream_user_data) {
  auto fuzzed_data_provider = static_cast<FuzzedDataProvider *>(conn_user_data);

  return fuzzed_data_provider->ConsumeBool() ? NGHTTP3_ERR_CALLBACK_FAILURE : 0;
}

static int recv_trailer(nghttp3_conn *conn, int64_t stream_id, int32_t token,
                        nghttp3_rcbuf *name, nghttp3_rcbuf *value,
                        uint8_t flags, void *conn_user_data,
                        void *stream_user_data) {
  auto fuzzed_data_provider = static_cast<FuzzedDataProvider *>(conn_user_data);

  return fuzzed_data_provider->ConsumeBool() ? NGHTTP3_ERR_CALLBACK_FAILURE : 0;
}

static int end_trailers(nghttp3_conn *conn, int64_t stream_id, int fin,
                        void *conn_user_data, void *stream_user_data) {
  auto fuzzed_data_provider = static_cast<FuzzedDataProvider *>(conn_user_data);

  return fuzzed_data_provider->ConsumeBool() ? NGHTTP3_ERR_CALLBACK_FAILURE : 0;
}

static int stop_sending(nghttp3_conn *conn, int64_t stream_id,
                        uint64_t app_error_code, void *conn_user_data,
                        void *stream_user_data) {
  auto fuzzed_data_provider = static_cast<FuzzedDataProvider *>(conn_user_data);

  return fuzzed_data_provider->ConsumeBool() ? NGHTTP3_ERR_CALLBACK_FAILURE : 0;
}

static int end_stream(nghttp3_conn *conn, int64_t stream_id,
                      void *conn_user_data, void *stream_user_data) {
  auto fuzzed_data_provider = static_cast<FuzzedDataProvider *>(conn_user_data);

  if (fuzzed_data_provider->ConsumeBool()) {
    return NGHTTP3_ERR_CALLBACK_FAILURE;
  }

  if (fuzzed_data_provider->ConsumeBool()) {
    return 0;
  }

  auto name = fuzzed_data_provider->ConsumeRandomLengthString();
  auto value = fuzzed_data_provider->ConsumeRandomLengthString();

  const nghttp3_nv nva[] = {
    {
      .name = (uint8_t *)name.c_str(),
      .value = (uint8_t *)value.c_str(),
      .namelen = name.size(),
      .valuelen = value.size(),
    },
  };

  return nghttp3_conn_submit_response(conn, stream_id, nva,
                                      nghttp3_arraylen(nva), nullptr);
}

static int reset_stream(nghttp3_conn *conn, int64_t stream_id,
                        uint64_t app_error_code, void *conn_user_data,
                        void *stream_user_data) {
  auto fuzzed_data_provider = static_cast<FuzzedDataProvider *>(conn_user_data);

  return fuzzed_data_provider->ConsumeBool() ? NGHTTP3_ERR_CALLBACK_FAILURE : 0;
}

static int shutdown(nghttp3_conn *conn, int64_t id, void *conn_user_data) {
  auto fuzzed_data_provider = static_cast<FuzzedDataProvider *>(conn_user_data);

  return fuzzed_data_provider->ConsumeBool() ? NGHTTP3_ERR_CALLBACK_FAILURE : 0;
}

static int recv_settings(nghttp3_conn *conn, const nghttp3_settings *settings,
                         void *conn_user_data) {
  auto fuzzed_data_provider = static_cast<FuzzedDataProvider *>(conn_user_data);

  return fuzzed_data_provider->ConsumeBool() ? NGHTTP3_ERR_CALLBACK_FAILURE : 0;
}

static void *fuzzed_malloc(size_t size, void *user_data) {
  auto fuzzed_data_provider = static_cast<FuzzedDataProvider *>(user_data);

  return fuzzed_data_provider->ConsumeBool() ? nullptr : malloc(size);
}

static void *fuzzed_calloc(size_t nmemb, size_t size, void *user_data) {
  auto fuzzed_data_provider = static_cast<FuzzedDataProvider *>(user_data);

  return fuzzed_data_provider->ConsumeBool() ? nullptr : calloc(nmemb, size);
}

static void *fuzzed_realloc(void *ptr, size_t size, void *user_data) {
  auto fuzzed_data_provider = static_cast<FuzzedDataProvider *>(user_data);

  return fuzzed_data_provider->ConsumeBool() ? nullptr : realloc(ptr, size);
}

static int send_data(nghttp3_conn *conn) {
  std::array<nghttp3_vec, 16> vec;
  int64_t stream_id;
  int fin;

  for (;;) {
    auto veccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec.data(),
                                             vec.size());
    if (veccnt < 0) {
      return veccnt;
    }

    if (veccnt || fin) {
      auto ndatalen = nghttp3_vec_len(vec.data(), veccnt);

      if (nghttp3_conn_add_write_offset(conn, stream_id, ndatalen) < 0) {
        return 0;
      }

      if (nghttp3_conn_add_ack_offset(conn, stream_id, ndatalen) < 0) {
        return 0;
      }
    } else {
      return 0;
    }
  }
}

static int set_stream_priorities(nghttp3_conn *conn,
                                 FuzzedDataProvider *fuzzed_data_provider) {
  for (; fuzzed_data_provider->ConsumeBool();) {
    auto stream_id = fuzzed_data_provider->ConsumeIntegralInRange<int64_t>(
      0, NGHTTP3_MAX_VARINT);

    nghttp3_pri pri{
      .urgency = fuzzed_data_provider->ConsumeIntegralInRange<uint32_t>(
        0, NGHTTP3_URGENCY_LEVELS - 1),
      .inc = fuzzed_data_provider->ConsumeIntegralInRange<uint8_t>(0, 1),
    };

    auto rv = nghttp3_conn_set_server_stream_priority(conn, stream_id, &pri);
    if (rv != 0) {
      return rv;
    }
  }

  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  FuzzedDataProvider fuzzed_data_provider(data, size);

  nghttp3_callbacks callbacks{
    .acked_stream_data = acked_stream_data,
    .stream_close = stream_close,
    .recv_data = recv_data,
    .deferred_consume = deferred_consume,
    .begin_headers = begin_headers,
    .recv_header = recv_header,
    .end_headers = end_headers,
    .begin_trailers = begin_trailers,
    .recv_trailer = recv_trailer,
    .end_trailers = end_trailers,
    .stop_sending = stop_sending,
    .end_stream = end_stream,
    .reset_stream = reset_stream,
    .shutdown = shutdown,
    .recv_settings = recv_settings,
  };

  nghttp3_settings settings;
  nghttp3_settings_default(&settings);
  settings.max_field_section_size =
    fuzzed_data_provider.ConsumeIntegralInRange<uint64_t>(0,
                                                          NGHTTP3_MAX_VARINT);
  settings.qpack_max_dtable_capacity =
    fuzzed_data_provider.ConsumeIntegralInRange<size_t>(0, NGHTTP3_MAX_VARINT);
  settings.qpack_encoder_max_dtable_capacity =
    fuzzed_data_provider.ConsumeIntegralInRange<size_t>(0, NGHTTP3_MAX_VARINT);
  settings.qpack_blocked_streams =
    fuzzed_data_provider.ConsumeIntegralInRange<size_t>(0, NGHTTP3_MAX_VARINT);
  settings.enable_connect_protocol =
    fuzzed_data_provider.ConsumeIntegral<uint8_t>();
  settings.h3_datagram = fuzzed_data_provider.ConsumeIntegral<uint8_t>();

  nghttp3_mem mem = *nghttp3_mem_default();
  mem.user_data = &fuzzed_data_provider;
  mem.malloc = fuzzed_malloc;
  mem.calloc = fuzzed_calloc;
  mem.realloc = fuzzed_realloc;

  nghttp3_conn *conn;
  auto rv = nghttp3_conn_server_new(&conn, &callbacks, &settings, &mem,
                                    &fuzzed_data_provider);
  if (rv != 0) {
    return 0;
  }

  nghttp3_conn_set_max_client_streams_bidi(
    conn, fuzzed_data_provider.ConsumeIntegral<uint64_t>());

  rv = nghttp3_conn_bind_qpack_streams(conn, 7, 11);
  if (rv != 0) {
    goto fin;
  }

  nghttp3_ssize nread;

  if (send_data(conn) != 0) {
    goto fin;
  }

  for (; fuzzed_data_provider.remaining_bytes() > 0;) {
    for (; fuzzed_data_provider.remaining_bytes() > 0 &&
           fuzzed_data_provider.ConsumeBool();) {
      auto stream_id = fuzzed_data_provider.ConsumeIntegralInRange<int64_t>(
        0, NGHTTP3_MAX_VARINT);
      if (nghttp3_server_stream_uni(stream_id)) {
        goto fin;
      }

      auto chunk_size = fuzzed_data_provider.ConsumeIntegral<size_t>();
      auto chunk = fuzzed_data_provider.ConsumeBytes<uint8_t>(chunk_size);
      auto fin = fuzzed_data_provider.ConsumeBool();

      nread = nghttp3_conn_read_stream(conn, stream_id, chunk.data(),
                                       chunk.size(), fin);
      if (nread < 0) {
        goto fin;
      }
    }

    if (set_stream_priorities(conn, &fuzzed_data_provider) != 0) {
      goto fin;
    }

    if (send_data(conn) != 0) {
      goto fin;
    }
  }

fin:
  nghttp3_conn_del(conn);

  return 0;
}
