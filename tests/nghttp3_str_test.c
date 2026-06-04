/*
 * nghttp3
 *
 * Copyright (c) 2026 nghttp3 contributors
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
#include "nghttp3_str_test.h"

#include <stdio.h>

#include "nghttp3_str.h"
#include "nghttp3_macro.h"
#include "nghttp3_test_helper.h"

static const MunitTest tests[] = {
  munit_void_test(test_nghttp3_downcase_byte),
  munit_test_end(),
};

const MunitSuite str_suite = {
  .prefix = "/str",
  .tests = tests,
};

void test_nghttp3_downcase_byte(void) {
  size_t i;

  for (i = 0; i < 256; ++i) {
    if ('A' <= i && i <= 'Z') {
      assert_uint8((uint8_t)(i - 'A' + 'a'), ==,
                   nghttp3_downcase_byte((uint8_t)i));
    } else {
      assert_uint8((uint8_t)i, ==, nghttp3_downcase_byte((uint8_t)i));
    }
  }
}
