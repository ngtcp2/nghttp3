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
#endif /* defined(HAVE_CONFIG_H) */

#include "munit.h"

/* include test cases' include files here */
#include "nghttp3_qpack_test.h"
#include "nghttp3_conn_test.h"
#include "nghttp3_stream_test.h"
#include "nghttp3_tnode_test.h"
#include "nghttp3_http_test.h"
#include "nghttp3_conv_test.h"
#include "nghttp3_settings_test.h"
#include "nghttp3_callbacks_test.h"

int main(int argc, char **argv) {
  const MunitSuite suites[] = {
    qpack_suite, conn_suite,     stream_suite,    tnode_suite,
    http_suite,  settings_suite, callbacks_suite, {0},
  };
  const MunitSuite suite = {
    .prefix = "",
    .suites = suites,
    .iterations = 1,
  };

  return munit_suite_main(&suite, NULL, argc, argv);
}
