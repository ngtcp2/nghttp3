#include <array>

#include <fuzzer/FuzzedDataProvider.h>

#include <nghttp3/nghttp3.h>

static int send_data(nghttp3_conn *conn) {
  std::array<nghttp3_vec, 16> vec;
  int64_t stream_id;
  int fin;

  for (;;) {
    auto veccnt = nghttp3_conn_writev_stream(conn, &stream_id, &fin, vec.data(),
                                             vec.size());
    if (veccnt < 0) {
      return 0;
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

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  FuzzedDataProvider fuzzed_data_provider(data, size);
  nghttp3_callbacks callbacks{};

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

  nghttp3_conn *conn;
  auto rv =
    nghttp3_conn_server_new(&conn, &callbacks, &settings, nullptr, nullptr);
  if (rv != 0) {
    return 0;
  }

  nghttp3_conn_set_max_client_streams_bidi(conn, 100);

  nghttp3_ssize nread;

  if (send_data(conn) != 0) {
    goto fin;
  }

  while (fuzzed_data_provider.remaining_bytes() > 0) {
    auto stream_id = fuzzed_data_provider.ConsumeIntegral<int64_t>();
    auto chunk_size = fuzzed_data_provider.ConsumeIntegral<size_t>();
    auto chunk = fuzzed_data_provider.ConsumeBytes<uint8_t>(chunk_size);
    auto fin = fuzzed_data_provider.ConsumeBool();

    nread = nghttp3_conn_read_stream(conn, stream_id, chunk.data(),
                                     chunk.size(), fin);
    if (nread < 0) {
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
