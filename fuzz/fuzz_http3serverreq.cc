#include <array>

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
  nghttp3_callbacks callbacks{};
  nghttp3_settings settings;

  nghttp3_settings_default(&settings);

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

  nread = nghttp3_conn_read_stream(conn, 0, data, size, 0);
  if (nread < 0) {
    goto fin;
  }

  if (send_data(conn) != 0) {
    goto fin;
  }

fin:
  nghttp3_conn_del(conn);

  return 0;
}
