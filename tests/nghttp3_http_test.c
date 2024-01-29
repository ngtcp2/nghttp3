/*
 * nghttp3
 *
 * Copyright (c) 2020 nghttp3 contributors
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
#include "nghttp3_http_test.h"

#include <stdio.h>
#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>

#include <cmocka.h>

#include "nghttp3_http.h"
#include "nghttp3_macro.h"
#include "nghttp3_test_helper.h"

void test_nghttp3_http_parse_priority(void **state) {
  int rv;
  (void)state;

  {
    nghttp3_pri pri = {(uint32_t)-1, UINT8_MAX};
    const uint8_t v[] = "";

    rv = nghttp3_http_parse_priority(&pri, v, sizeof(v) - 1);

    assert_int_equal(0, rv);
    assert_int_equal((uint32_t)-1, pri.urgency);
    assert_int_equal(UINT8_MAX, pri.inc);
  }

  // Check API version.
  {
    nghttp3_pri pri = {(uint32_t)-1, UINT8_MAX};
    const uint8_t v[] = "";

    rv = nghttp3_pri_parse_priority(&pri, v, sizeof(v) - 1);

    assert_int_equal(0, rv);
    assert_int_equal((uint32_t)-1, pri.urgency);
    assert_int_equal(UINT8_MAX, pri.inc);
  }

  {
    nghttp3_pri pri = {(uint32_t)-1, UINT8_MAX};
    const uint8_t v[] = "u=1";

    rv = nghttp3_http_parse_priority(&pri, v, sizeof(v) - 1);

    assert_int_equal(0, rv);
    assert_int_equal(1, pri.urgency);
    assert_int_equal(UINT8_MAX, pri.inc);
  }

  {
    nghttp3_pri pri = {(uint32_t)-1, UINT8_MAX};
    const uint8_t v[] = "i";

    rv = nghttp3_http_parse_priority(&pri, v, sizeof(v) - 1);

    assert_int_equal(0, rv);
    assert_int_equal((uint32_t)-1, pri.urgency);
    assert_int_equal(1, pri.inc);
  }

  {
    nghttp3_pri pri = {(uint32_t)-1, UINT8_MAX};
    const uint8_t v[] = "i=?0";

    rv = nghttp3_http_parse_priority(&pri, v, sizeof(v) - 1);

    assert_int_equal(0, rv);
    assert_int_equal((uint32_t)-1, pri.urgency);
    assert_int_equal(0, pri.inc);
  }

  {
    nghttp3_pri pri = {(uint32_t)-1, UINT8_MAX};
    const uint8_t v[] = "u=7,i";

    rv = nghttp3_http_parse_priority(&pri, v, sizeof(v) - 1);

    assert_int_equal(0, rv);
    assert_int_equal(7, pri.urgency);
    assert_int_equal(1, pri.inc);
  }

  {
    nghttp3_pri pri = {(uint32_t)-1, UINT8_MAX};
    const uint8_t v[] = "u=0,i=?0";

    rv = nghttp3_http_parse_priority(&pri, v, sizeof(v) - 1);

    assert_int_equal(0, rv);
    assert_int_equal(0, pri.urgency);
    assert_int_equal(0, pri.inc);
  }

  {
    nghttp3_pri pri = {(uint32_t)-1, UINT8_MAX};
    const uint8_t v[] = "u=3, i";

    rv = nghttp3_http_parse_priority(&pri, v, sizeof(v) - 1);

    assert_int_equal(0, rv);
    assert_int_equal(3, pri.urgency);
    assert_int_equal(1, pri.inc);
  }

  {
    nghttp3_pri pri = {(uint32_t)-1, UINT8_MAX};
    const uint8_t v[] = "u=0, i, i=?0, u=6";

    rv = nghttp3_http_parse_priority(&pri, v, sizeof(v) - 1);

    assert_int_equal(0, rv);
    assert_int_equal(6, pri.urgency);
    assert_int_equal(0, pri.inc);
  }

  {
    nghttp3_pri pri = {(uint32_t)-1, UINT8_MAX};
    const uint8_t v[] = "u=0,";

    rv = nghttp3_http_parse_priority(&pri, v, sizeof(v) - 1);

    assert_int_equal(NGHTTP3_ERR_INVALID_ARGUMENT, rv);
  }

  {
    nghttp3_pri pri = {(uint32_t)-1, UINT8_MAX};
    const uint8_t v[] = "u=0, ";

    rv = nghttp3_http_parse_priority(&pri, v, sizeof(v) - 1);

    assert_int_equal(NGHTTP3_ERR_INVALID_ARGUMENT, rv);
  }

  {
    nghttp3_pri pri = {(uint32_t)-1, UINT8_MAX};
    const uint8_t v[] = "u=";

    rv = nghttp3_http_parse_priority(&pri, v, sizeof(v) - 1);

    assert_int_equal(NGHTTP3_ERR_INVALID_ARGUMENT, rv);
  }

  {
    nghttp3_pri pri = {(uint32_t)-1, UINT8_MAX};
    const uint8_t v[] = "u";

    rv = nghttp3_http_parse_priority(&pri, v, sizeof(v) - 1);

    assert_int_equal(NGHTTP3_ERR_INVALID_ARGUMENT, rv);
  }

  {
    nghttp3_pri pri = {(uint32_t)-1, UINT8_MAX};
    const uint8_t v[] = "i=?1";

    rv = nghttp3_http_parse_priority(&pri, v, sizeof(v) - 1);

    assert_int_equal(0, rv);
    assert_int_equal((uint32_t)-1, pri.urgency);
    assert_int_equal(1, pri.inc);
  }

  {
    nghttp3_pri pri = {(uint32_t)-1, UINT8_MAX};
    const uint8_t v[] = "i=?2";

    rv = nghttp3_http_parse_priority(&pri, v, sizeof(v) - 1);

    assert_int_equal(NGHTTP3_ERR_INVALID_ARGUMENT, rv);
  }

  {
    nghttp3_pri pri = {(uint32_t)-1, UINT8_MAX};
    const uint8_t v[] = "i=?";

    rv = nghttp3_http_parse_priority(&pri, v, sizeof(v) - 1);

    assert_int_equal(NGHTTP3_ERR_INVALID_ARGUMENT, rv);
  }

  {
    nghttp3_pri pri = {(uint32_t)-1, UINT8_MAX};
    const uint8_t v[] = "i=";

    rv = nghttp3_http_parse_priority(&pri, v, sizeof(v) - 1);

    assert_int_equal(NGHTTP3_ERR_INVALID_ARGUMENT, rv);
  }

  {
    nghttp3_pri pri = {(uint32_t)-1, UINT8_MAX};
    const uint8_t v[] = "u=-1";

    rv = nghttp3_http_parse_priority(&pri, v, sizeof(v) - 1);

    assert_int_equal(NGHTTP3_ERR_INVALID_ARGUMENT, rv);
  }

  {
    nghttp3_pri pri = {(uint32_t)-1, UINT8_MAX};
    const uint8_t v[] = "u=8";

    rv = nghttp3_http_parse_priority(&pri, v, sizeof(v) - 1);

    assert_int_equal(NGHTTP3_ERR_INVALID_ARGUMENT, rv);
  }

  {
    nghttp3_pri pri = {(uint32_t)-1, UINT8_MAX};
    const uint8_t v[] =
        "i=?0, u=1, a=(x y z), u=2; i=?0;foo=\",,,\", i=?1;i=?0; u=6";

    rv = nghttp3_http_parse_priority(&pri, v, sizeof(v) - 1);

    assert_int_equal(0, rv);
    assert_int_equal(2, pri.urgency);
    assert_int_equal(1, pri.inc);
  }

  {
    nghttp3_pri pri = {(uint32_t)-1, UINT8_MAX};
    const uint8_t v[] = {'u', '='};

    rv = nghttp3_http_parse_priority(&pri, v, sizeof(v));

    assert_int_equal(NGHTTP3_ERR_INVALID_ARGUMENT, rv);
  }
}

#define check_header_value(S)                                                  \
  nghttp3_check_header_value((const uint8_t *)S, sizeof(S) - 1)

void test_nghttp3_check_header_value(void **state) {
  uint8_t goodval[] = {'a', 'b', 0x80u, 'c', 0xffu, 'd'};
  uint8_t badval1[] = {'a', 0x1fu, 'b'};
  uint8_t badval2[] = {'a', 0x7fu, 'b'};
  (void)state;

  assert_true(check_header_value("!|}~"));
  assert_false(check_header_value(" !|}~"));
  assert_false(check_header_value("!|}~ "));
  assert_false(check_header_value("\t!|}~"));
  assert_false(check_header_value("!|}~\t"));
  assert_true(check_header_value(goodval));
  assert_false(check_header_value(badval1));
  assert_false(check_header_value(badval2));
  assert_true(check_header_value(""));
  assert_false(check_header_value(" "));
  assert_false(check_header_value("\t"));
}
