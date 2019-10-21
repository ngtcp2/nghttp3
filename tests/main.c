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

#include <stdio.h>
#include <string.h>
#include <CUnit/Basic.h>
/* include test cases' include files here */
#include "nghttp3_qpack_test.h"
#include "nghttp3_conn_test.h"
#include "nghttp3_tnode_test.h"

static int init_suite1(void) { return 0; }

static int clean_suite1(void) { return 0; }

int main() {
  CU_pSuite pSuite = NULL;
  unsigned int num_tests_failed;

  /* initialize the CUnit test registry */
  if (CUE_SUCCESS != CU_initialize_registry())
    return (int)CU_get_error();

  /* add a suite to the registry */
  pSuite = CU_add_suite("libnghttp3_TestSuite", init_suite1, clean_suite1);
  if (NULL == pSuite) {
    CU_cleanup_registry();
    return (int)CU_get_error();
  }

  /* add the tests to the suite */
  if (!CU_add_test(pSuite, "qpack_encoder_encode",
                   test_nghttp3_qpack_encoder_encode) ||
      !CU_add_test(pSuite, "qpack_encoder_still_blocked",
                   test_nghttp3_qpack_encoder_still_blocked) ||
      !CU_add_test(pSuite, "qpack_encoder_set_dtable_cap",
                   test_nghttp3_qpack_encoder_set_dtable_cap) ||
      !CU_add_test(pSuite, "qpack_decoder_feedback",
                   test_nghttp3_qpack_decoder_feedback) ||
      !CU_add_test(pSuite, "qpack_huffman", test_nghttp3_qpack_huffman) ||
      !CU_add_test(pSuite, "conn_read_control",
                   test_nghttp3_conn_read_control) ||
      !CU_add_test(pSuite, "conn_write_control",
                   test_nghttp3_conn_write_control) ||
      !CU_add_test(pSuite, "conn_submit_request",
                   test_nghttp3_conn_submit_request) ||
      !CU_add_test(pSuite, "conn_submit_push_promise",
                   test_nghttp3_conn_submit_push_promise) ||
      !CU_add_test(pSuite, "conn_http_request",
                   test_nghttp3_conn_http_request) ||
      !CU_add_test(pSuite, "conn_http_resp_header",
                   test_nghttp3_conn_http_resp_header) ||
      !CU_add_test(pSuite, "conn_http_req_header",
                   test_nghttp3_conn_http_req_header) ||
      !CU_add_test(pSuite, "conn_http_content_length",
                   test_nghttp3_conn_http_content_length) ||
      !CU_add_test(pSuite, "conn_http_content_length_with_data",
                   test_nghttp3_conn_http_content_length_with_data) ||
      !CU_add_test(pSuite, "conn_http_content_length_mismatch",
                   test_nghttp3_conn_http_content_length_mismatch) ||
      !CU_add_test(pSuite, "conn_http_non_final_response",
                   test_nghttp3_conn_http_non_final_response) ||
      !CU_add_test(pSuite, "conn_http_trailers",
                   test_nghttp3_conn_http_trailers) ||
      !CU_add_test(pSuite, "conn_http_ignore_content_length",
                   test_nghttp3_conn_http_ignore_content_length) ||
      !CU_add_test(pSuite, "conn_http_record_request_method",
                   test_nghttp3_conn_http_record_request_method) ||
      !CU_add_test(pSuite, "conn_qpack_blocked_stream",
                   test_nghttp3_conn_qpack_blocked_stream) ||
      !CU_add_test(pSuite, "conn_recv_cancel_push",
                   test_nghttp3_conn_recv_cancel_push) ||
      !CU_add_test(pSuite, "conn_cancel_push", test_nghttp3_conn_cancel_push) ||
      !CU_add_test(pSuite, "conn_recv_push_promise",
                   test_nghttp3_conn_recv_push_promise) ||
      !CU_add_test(pSuite, "conn_recv_push_stream",
                   test_nghttp3_conn_recv_push_stream) ||
      !CU_add_test(pSuite, "conn_submit_response_read_blocked",
                   test_nghttp3_conn_submit_response_read_blocked) ||
      !CU_add_test(pSuite, "conn_just_fin", test_nghttp3_conn_just_fin) ||
      !CU_add_test(pSuite, "tnode_mutation", test_nghttp3_tnode_mutation) ||
      !CU_add_test(pSuite, "tnode_schedule", test_nghttp3_tnode_schedule)) {
    CU_cleanup_registry();
    return (int)CU_get_error();
  }

  /* Run all tests using the CUnit Basic interface */
  CU_basic_set_mode(CU_BRM_VERBOSE);
  CU_basic_run_tests();
  num_tests_failed = CU_get_number_of_tests_failed();
  CU_cleanup_registry();
  if (CU_get_error() == CUE_SUCCESS) {
    return (int)num_tests_failed;
  } else {
    printf("CUnit Error: %s\n", CU_get_error_msg());
    return (int)CU_get_error();
  }
}
