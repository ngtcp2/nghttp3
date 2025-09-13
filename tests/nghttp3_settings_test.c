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
#include "nghttp3_settings_test.h"

#include <stdio.h>

#include "nghttp3_settings.h"
#include "nghttp3_test_helper.h"

static const MunitTest tests[] = {
  munit_void_test(test_nghttp3_settings_convert_to_latest),
  munit_void_test(test_nghttp3_settings_convert_to_old),
  munit_test_end(),
};

const MunitSuite settings_suite = {
  .prefix = "/settings",
  .tests = tests,
};

void test_nghttp3_settings_convert_to_latest(void) {
  nghttp3_settings *src, srcbuf, settingsbuf;
  const nghttp3_settings *dest;
  const uint8_t origins[] = "foo";
  nghttp3_vec origin_list = {
    .base = (uint8_t *)origins,
    .len = strsize(origins),
  };
  size_t v2len;

  nghttp3_settings_default_versioned(NGHTTP3_SETTINGS_V2, &srcbuf);

  srcbuf.max_field_section_size = 1000000007;
  srcbuf.qpack_max_dtable_capacity = 1000000009;
  srcbuf.qpack_encoder_max_dtable_capacity = 781506803;
  srcbuf.qpack_blocked_streams = 478324193;
  srcbuf.enable_connect_protocol = 1;
  srcbuf.h3_datagram = 1;
  srcbuf.origin_list = &origin_list;

  v2len = nghttp3_settingslen_version(NGHTTP3_SETTINGS_V2);

  src = malloc(v2len);

  memcpy(src, &srcbuf, v2len);

  dest =
    nghttp3_settings_convert_to_latest(&settingsbuf, NGHTTP3_SETTINGS_V2, src);

  free(src);

  assert_ptr_equal(&settingsbuf, dest);
  assert_uint64(srcbuf.max_field_section_size, ==,
                dest->max_field_section_size);
  assert_size(srcbuf.qpack_max_dtable_capacity, ==,
              dest->qpack_max_dtable_capacity);
  assert_size(srcbuf.qpack_encoder_max_dtable_capacity, ==,
              dest->qpack_encoder_max_dtable_capacity);
  assert_size(srcbuf.qpack_blocked_streams, ==, dest->qpack_blocked_streams);
  assert_uint8(srcbuf.enable_connect_protocol, ==,
               dest->enable_connect_protocol);
  assert_uint8(srcbuf.h3_datagram, ==, dest->h3_datagram);
  assert_ptr_equal(srcbuf.origin_list, dest->origin_list);
  assert_uint64(UINT64_MAX, ==, dest->initial_ts);
  assert_uint64(NGHTTP3_DEFAULT_GLITCH_RATELIM_BURST, ==,
                dest->glitch_ratelim_burst);
  assert_uint64(NGHTTP3_DEFAULT_GLITCH_RATELIM_RATE, ==,
                dest->glitch_ratelim_rate);
}

void test_nghttp3_settings_convert_to_old(void) {
  nghttp3_settings src, *dest, destbuf;
  const uint8_t origins[] = "foo";
  nghttp3_vec origin_list = {
    .base = (uint8_t *)origins,
    .len = strsize(origins),
  };
  size_t v2len;

  v2len = nghttp3_settingslen_version(NGHTTP3_SETTINGS_V2);

  dest = malloc(v2len);

  nghttp3_settings_default(&src);
  src.max_field_section_size = 1000000007;
  src.qpack_max_dtable_capacity = 1000000009;
  src.qpack_encoder_max_dtable_capacity = 781506803;
  src.qpack_blocked_streams = 478324193;
  src.enable_connect_protocol = 1;
  src.h3_datagram = 1;
  src.origin_list = &origin_list;
  src.initial_ts = 6398888;
  src.glitch_ratelim_burst = 74111;
  src.glitch_ratelim_rate = 6831;

  nghttp3_settings_convert_to_old(NGHTTP3_SETTINGS_V2, dest, &src);

  memset(&destbuf, 0, sizeof(destbuf));
  memcpy(&destbuf, dest, v2len);

  free(dest);

  assert_uint64(src.max_field_section_size, ==, destbuf.max_field_section_size);
  assert_size(src.qpack_max_dtable_capacity, ==,
              destbuf.qpack_max_dtable_capacity);
  assert_size(src.qpack_encoder_max_dtable_capacity, ==,
              destbuf.qpack_encoder_max_dtable_capacity);
  assert_size(src.qpack_blocked_streams, ==, destbuf.qpack_blocked_streams);
  assert_uint8(src.enable_connect_protocol, ==,
               destbuf.enable_connect_protocol);
  assert_uint8(src.h3_datagram, ==, destbuf.h3_datagram);
  assert_ptr_equal(src.origin_list, destbuf.origin_list);
  assert_uint64(0, ==, destbuf.initial_ts);
  assert_uint64(0, ==, destbuf.glitch_ratelim_burst);
  assert_uint64(0, ==, destbuf.glitch_ratelim_rate);
}
