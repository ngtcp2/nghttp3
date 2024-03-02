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
#ifndef NGHTTP3_QPACK_TEST_H
#define NGHTTP3_QPACK_TEST_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#define MUNIT_ENABLE_ASSERT_ALIASES

#include "munit.h"

extern const MunitSuite qpack_suite;

munit_void_test_decl(test_nghttp3_qpack_encoder_encode);
munit_void_test_decl(test_nghttp3_qpack_encoder_encode_try_encode);
munit_void_test_decl(test_nghttp3_qpack_encoder_still_blocked);
munit_void_test_decl(test_nghttp3_qpack_encoder_set_dtable_cap);
munit_void_test_decl(test_nghttp3_qpack_decoder_feedback);
munit_void_test_decl(test_nghttp3_qpack_decoder_stream_overflow);
munit_void_test_decl(test_nghttp3_qpack_huffman);
munit_void_test_decl(test_nghttp3_qpack_huffman_decode_failure_state);
munit_void_test_decl(test_nghttp3_qpack_decoder_reconstruct_ricnt);
munit_void_test_decl(test_nghttp3_qpack_decoder_read_encoder);
munit_void_test_decl(test_nghttp3_qpack_encoder_read_decoder);

#endif /* NGHTTP3_QPACK_TEST_H */
