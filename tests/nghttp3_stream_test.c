/*
 * nghttp3
 *
 * Copyright (c) 2025 nghttp3 contributors
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
#include "nghttp3_stream_test.h"

#include <stdio.h>
#include <assert.h>

#include "nghttp3_stream.h"
#include "nghttp3_test_helper.h"

static const MunitTest tests[] = {
  munit_void_test(test_nghttp3_read_varint),
  munit_test_end(),
};

const MunitSuite stream_suite = {
  .prefix = "/stream",
  .tests = tests,
};

void test_nghttp3_read_varint(void) {
  nghttp3_varint_read_state rvint;
  nghttp3_ssize nread;

  {
    /* 1 byte integer */
    const uint8_t input[] = {0x3f};

    nghttp3_varint_read_state_reset(&rvint);

    nread =
      nghttp3_read_varint(&rvint, input, input + sizeof(input), /* fin = */ 0);

    assert_ptrdiff(1, ==, nread);
    assert_size(0, ==, rvint.left);
    assert_int64(63, ==, rvint.acc);
  }

  {
    /* 1 byte integer with fin */
    const uint8_t input[] = {0x3f};

    nghttp3_varint_read_state_reset(&rvint);

    nread =
      nghttp3_read_varint(&rvint, input, input + sizeof(input), /* fin = */ 1);

    assert_ptrdiff((nghttp3_ssize)sizeof(input), ==, nread);
    assert_size(0, ==, rvint.left);
    assert_int64(63, ==, rvint.acc);
  }

  {
    /* 4 bytes integer */
    const uint8_t input[] = {0xad, 0xa5, 0xcb, 0x03};

    nghttp3_varint_read_state_reset(&rvint);

    nread =
      nghttp3_read_varint(&rvint, input, input + sizeof(input), /* fin = */ 0);

    assert_ptrdiff((nghttp3_ssize)sizeof(input), ==, nread);
    assert_size(0, ==, rvint.left);
    assert_int64(0x2d << 24 | 0xa5 << 16 | 0xcb << 8 | 0x03, ==, rvint.acc);
  }

  {
    /* 4 bytes integer but incomplete */
    const uint8_t input[] = {0xad, 0xa5, 0xcb, 0x03};

    nghttp3_varint_read_state_reset(&rvint);

    nread = nghttp3_read_varint(&rvint, input, input + sizeof(input) - 1,
                                /* fin = */ 0);

    assert_ptrdiff((nghttp3_ssize)(sizeof(input) - 1), ==, nread);
    assert_size(1, ==, rvint.left);

    nread = nghttp3_read_varint(&rvint, input + sizeof(input) - 1,
                                input + sizeof(input), /* fin = */ 1);

    assert_ptrdiff(1, ==, nread);
    assert_size(0, ==, rvint.left);
    assert_int64(0x2d << 24 | 0xa5 << 16 | 0xcb << 8 | 0x03, ==, rvint.acc);
  }

  {
    /* 4 bytes integer prematurely ended by fin */
    const uint8_t input[] = {0xad, 0xa5, 0xcb};

    nghttp3_varint_read_state_reset(&rvint);

    nread =
      nghttp3_read_varint(&rvint, input, input + sizeof(input), /* fin = */ 1);

    assert_ptrdiff(NGHTTP3_ERR_INVALID_ARGUMENT, ==, nread);
  }

  {
    /* 4 bytes integer prematurely ended by fin in the second input */
    const uint8_t input[] = {0xad, 0xa5, 0xcb};

    nghttp3_varint_read_state_reset(&rvint);

    nread = nghttp3_read_varint(&rvint, input, input + (sizeof(input) - 1),
                                /* fin = */ 0);

    assert_ptrdiff((nghttp3_ssize)(sizeof(input) - 1), ==, nread);
    assert_size(2, ==, rvint.left);

    nread = nghttp3_read_varint(&rvint, input + (sizeof(input) - 1),
                                input + sizeof(input),
                                /* fin = */ 1);

    assert_ptrdiff(NGHTTP3_ERR_INVALID_ARGUMENT, ==, nread);
  }

  {
    /* 4 bytes integer + extra byte */
    const uint8_t input[] = {0xad, 0xa5, 0xcb, 0x03, 0xff};

    nghttp3_varint_read_state_reset(&rvint);

    nread =
      nghttp3_read_varint(&rvint, input, input + sizeof(input), /* fin = */ 0);

    assert_ptrdiff((nghttp3_ssize)(sizeof(input) - 1), ==, nread);
    assert_size(0, ==, rvint.left);
    assert_int64(0x2d << 24 | 0xa5 << 16 | 0xcb << 8 | 0x03, ==, rvint.acc);
  }

  {
    /* 8 bytes integer */
    const uint8_t input[] = {0xed, 0xa5, 0xcb, 0x03, 0x90, 0xfc, 0x13, 0xd8};

    nghttp3_varint_read_state_reset(&rvint);

    nread =
      nghttp3_read_varint(&rvint, input, input + sizeof(input), /* fin = */ 0);

    assert_ptrdiff((nghttp3_ssize)sizeof(input), ==, nread);
    assert_size(0, ==, rvint.left);
    assert_int64(0x2dll << 56 | 0xa5ll << 48 | 0xcbll << 40 | 0x03ll << 32 |
                   0x90ll << 24 | 0xfcll << 16 | 0x13ll << 8 | 0xd8ll,
                 ==, rvint.acc);
  }

  {
    /* 8 bytes integer prematurely ended by fin at the first byte */
    const uint8_t input[] = {0xed};

    nghttp3_varint_read_state_reset(&rvint);

    nread =
      nghttp3_read_varint(&rvint, input, input + sizeof(input), /* fin = */ 1);

    assert_ptrdiff(NGHTTP3_ERR_INVALID_ARGUMENT, ==, nread);
  }
}
