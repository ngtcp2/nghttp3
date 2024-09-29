/*
 * nghttp3
 *
 * Copyright (c) 2024 nghttp3 contributors
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
#include <assert.h>

#include "nghttp3_str.h"
#include "nghttp3_test_helper.h"

static const MunitTest tests[] = {
  munit_void_test(test_nghttp3_find_first_of_sse42),
  munit_test_end(),
};

const MunitSuite str_suite = {
  "/str", tests, NULL, 1, MUNIT_SUITE_OPTION_NONE,
};

void test_nghttp3_find_first_of_sse42(void) {
#ifdef __SSE4_2__
  {
    const uint8_t s[] = "...............C";
    const uint8_t r[16] = "AZ";
    const uint8_t *o = nghttp3_find_first_of_sse42(s, s + sizeof(s) - 1, r, 2);

    assert_ptr_equal(o, s + 15);
  }

  {
    const uint8_t s[] = "......a..........C..............";
    const uint8_t r[16] = "AZ";
    const uint8_t *o = nghttp3_find_first_of_sse42(s, s + sizeof(s) - 1, r, 2);

    assert_ptr_equal(o, s + 17);
  }

  {
    const uint8_t s[] = "................";
    const uint8_t r[16] = "AZ";
    const uint8_t *o = nghttp3_find_first_of_sse42(s, s + sizeof(s) - 1, r, 2);

    assert_ptr_equal(o, s + 16);
  }
#endif /* __SSE4_2__ */
}
