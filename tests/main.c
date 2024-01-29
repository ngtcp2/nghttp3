/*
 * nghttp3
 *
 * Copyright (c) 2019 nghttp3 contributors
 * Copyright (c) 2016 ngtcp2 contributors
 * Copyright (c) 2012 nghttp2 contributors
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
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <cmocka.h>
/* include test cases' include files here */
#include "nghttp3_qpack_test.h"
#include "nghttp3_conn_test.h"
#include "nghttp3_tnode_test.h"
#include "nghttp3_http_test.h"
#include "nghttp3_conv_test.h"

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_nghttp3_qpack_encoder_encode),
      cmocka_unit_test(test_nghttp3_qpack_encoder_encode_try_encode),
      cmocka_unit_test(test_nghttp3_qpack_encoder_still_blocked),
      cmocka_unit_test(test_nghttp3_qpack_encoder_set_dtable_cap),
      cmocka_unit_test(test_nghttp3_qpack_decoder_feedback),
      cmocka_unit_test(test_nghttp3_qpack_decoder_stream_overflow),
      cmocka_unit_test(test_nghttp3_qpack_huffman),
      cmocka_unit_test(test_nghttp3_qpack_huffman_decode_failure_state),
      cmocka_unit_test(test_nghttp3_qpack_decoder_reconstruct_ricnt),
      cmocka_unit_test(test_nghttp3_conn_read_control),
      cmocka_unit_test(test_nghttp3_conn_write_control),
      cmocka_unit_test(test_nghttp3_conn_submit_request),
      cmocka_unit_test(test_nghttp3_conn_http_request),
      cmocka_unit_test(test_nghttp3_conn_http_resp_header),
      cmocka_unit_test(test_nghttp3_conn_http_req_header),
      cmocka_unit_test(test_nghttp3_conn_http_content_length),
      cmocka_unit_test(test_nghttp3_conn_http_content_length_mismatch),
      cmocka_unit_test(test_nghttp3_conn_http_non_final_response),
      cmocka_unit_test(test_nghttp3_conn_http_trailers),
      cmocka_unit_test(test_nghttp3_conn_http_ignore_content_length),
      cmocka_unit_test(test_nghttp3_conn_http_record_request_method),
      cmocka_unit_test(test_nghttp3_conn_http_error),
      cmocka_unit_test(test_nghttp3_conn_qpack_blocked_stream),
      cmocka_unit_test(test_nghttp3_conn_submit_response_read_blocked),
      cmocka_unit_test(test_nghttp3_conn_just_fin),
      cmocka_unit_test(test_nghttp3_conn_recv_uni),
      cmocka_unit_test(test_nghttp3_conn_recv_goaway),
      cmocka_unit_test(test_nghttp3_conn_shutdown_server),
      cmocka_unit_test(test_nghttp3_conn_shutdown_client),
      cmocka_unit_test(test_nghttp3_conn_priority_update),
      cmocka_unit_test(test_nghttp3_conn_request_priority),
      cmocka_unit_test(test_nghttp3_conn_set_stream_priority),
      cmocka_unit_test(test_nghttp3_conn_shutdown_stream_read),
      cmocka_unit_test(test_nghttp3_conn_stream_data_overflow),
      cmocka_unit_test(test_nghttp3_conn_get_frame_payload_left),
      cmocka_unit_test(test_nghttp3_tnode_schedule),
      cmocka_unit_test(test_nghttp3_http_parse_priority),
      cmocka_unit_test(test_nghttp3_check_header_value),
  };

  return cmocka_run_group_tests(tests, NULL, NULL);
}
