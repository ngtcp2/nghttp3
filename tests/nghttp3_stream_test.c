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
  munit_void_test(test_nghttp3_stream_buffer_data),
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
    static const uint8_t input[] = {0x3F};

    nghttp3_varint_read_state_reset(&rvint);

    nread =
      nghttp3_read_varint(&rvint, input, input + sizeof(input), /* fin = */ 0);

    assert_ptrdiff(1, ==, nread);
    assert_size(0, ==, rvint.left);
    assert_uint64(63, ==, rvint.acc);
  }

  {
    /* 1 byte integer with fin */
    static const uint8_t input[] = {0x3F};

    nghttp3_varint_read_state_reset(&rvint);

    nread =
      nghttp3_read_varint(&rvint, input, input + sizeof(input), /* fin = */ 1);

    assert_ptrdiff((nghttp3_ssize)sizeof(input), ==, nread);
    assert_size(0, ==, rvint.left);
    assert_uint64(63, ==, rvint.acc);
  }

  {
    /* 4 bytes integer */
    static const uint8_t input[] = {0xAD, 0xA5, 0xCB, 0x03};

    nghttp3_varint_read_state_reset(&rvint);

    nread =
      nghttp3_read_varint(&rvint, input, input + sizeof(input), /* fin = */ 0);

    assert_ptrdiff((nghttp3_ssize)sizeof(input), ==, nread);
    assert_size(0, ==, rvint.left);
    assert_uint64(0x2D << 24 | 0xA5 << 16 | 0xCB << 8 | 0x03, ==, rvint.acc);
  }

  {
    /* 4 bytes integer but incomplete */
    static const uint8_t input[] = {0xAD, 0xA5, 0xCB, 0x03};

    nghttp3_varint_read_state_reset(&rvint);

    nread = nghttp3_read_varint(&rvint, input, input + sizeof(input) - 1,
                                /* fin = */ 0);

    assert_ptrdiff((nghttp3_ssize)(sizeof(input) - 1), ==, nread);
    assert_size(1, ==, rvint.left);

    nread = nghttp3_read_varint(&rvint, input + sizeof(input) - 1,
                                input + sizeof(input), /* fin = */ 1);

    assert_ptrdiff(1, ==, nread);
    assert_size(0, ==, rvint.left);
    assert_uint64(0x2D << 24 | 0xA5 << 16 | 0xCB << 8 | 0x03, ==, rvint.acc);
  }

  {
    /* 4 bytes integer prematurely ended by fin */
    static const uint8_t input[] = {0xAD, 0xA5, 0xCB};

    nghttp3_varint_read_state_reset(&rvint);

    nread =
      nghttp3_read_varint(&rvint, input, input + sizeof(input), /* fin = */ 1);

    assert_ptrdiff(NGHTTP3_ERR_INVALID_ARGUMENT, ==, nread);
  }

  {
    /* 4 bytes integer prematurely ended by fin in the second input */
    static const uint8_t input[] = {0xAD, 0xA5, 0xCB};

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
    static const uint8_t input[] = {0xAD, 0xA5, 0xCB, 0x03, 0xFF};

    nghttp3_varint_read_state_reset(&rvint);

    nread =
      nghttp3_read_varint(&rvint, input, input + sizeof(input), /* fin = */ 0);

    assert_ptrdiff((nghttp3_ssize)(sizeof(input) - 1), ==, nread);
    assert_size(0, ==, rvint.left);
    assert_uint64(0x2D << 24 | 0xA5 << 16 | 0xCB << 8 | 0x03, ==, rvint.acc);
  }

  {
    /* 8 bytes integer */
    static const uint8_t input[] = {0xED, 0xA5, 0xCB, 0x03,
                                    0x90, 0xFC, 0x13, 0xD8};

    nghttp3_varint_read_state_reset(&rvint);

    nread =
      nghttp3_read_varint(&rvint, input, input + sizeof(input), /* fin = */ 0);

    assert_ptrdiff((nghttp3_ssize)sizeof(input), ==, nread);
    assert_size(0, ==, rvint.left);
    assert_uint64(0x2DLL << 56 | 0xA5LL << 48 | 0xCBLL << 40 | 0x03LL << 32 |
                    0x90LL << 24 | 0xFCLL << 16 | 0x13LL << 8 | 0xD8LL,
                  ==, rvint.acc);
  }

  {
    /* 8 bytes integer prematurely ended by fin at the first byte */
    static const uint8_t input[] = {0xED};

    nghttp3_varint_read_state_reset(&rvint);

    nread =
      nghttp3_read_varint(&rvint, input, input + sizeof(input), /* fin = */ 1);

    assert_ptrdiff(NGHTTP3_ERR_INVALID_ARGUMENT, ==, nread);
  }
}

void test_nghttp3_stream_buffer_data(void) {
  const nghttp3_mem *mem = nghttp3_mem_default();
  nghttp3_objalloc out_chunk_objalloc;
  nghttp3_objalloc stream_objalloc;
  nghttp3_stream *stream;
  static uint8_t data[NGHTTP3_STREAM_CAP_INQ_BUFLEN * 2 + 1];
  nghttp3_buf *buf;
  size_t i;
  size_t nalloc;
  int rv;

  for (i = 0; i < sizeof(data); ++i) {
    data[i] = (uint8_t)i;
  }

  nghttp3_objalloc_init(&out_chunk_objalloc, NGHTTP3_STREAM_MIN_CHUNK_SIZE * 16,
                        mem);
  nghttp3_objalloc_stream_init(&stream_objalloc, 8, mem);

  /* Small data must not allocate a large buffer. */
  rv = nghttp3_stream_new(&stream, 0, NULL, &out_chunk_objalloc,
                          &stream_objalloc, mem);

  assert_int(0, ==, rv);

  rv = nghttp3_stream_buffer_data(stream, data, 1);

  assert_int(0, ==, rv);
  assert_size(1, ==, nghttp3_ringbuf_len(&stream->inq));

  buf = nghttp3_ringbuf_get(&stream->inq, 0);

  assert_size(NGHTTP3_STREAM_MIN_INQ_BUFLEN, ==, nghttp3_buf_cap(buf));
  assert_size(1, ==, nghttp3_stream_get_buffered_datalen(stream));

  /* Data that arrive in small pieces fill the existing buffer first,
     and then the size of buffer is doubled up to
     NGHTTP3_STREAM_CAP_INQ_BUFLEN.  The allocated memory stays
     proportional to the amount of buffered data. */
  for (i = 1; i < NGHTTP3_STREAM_CAP_INQ_BUFLEN * 2; ++i) {
    rv = nghttp3_stream_buffer_data(stream, &data[i], 1);

    assert_int(0, ==, rv);
  }

  assert_size(NGHTTP3_STREAM_CAP_INQ_BUFLEN * 2, ==,
              nghttp3_stream_get_buffered_datalen(stream));
  assert_size(8, ==, nghttp3_ringbuf_len(&stream->inq));

  nalloc = 0;

  for (i = 0; i < nghttp3_ringbuf_len(&stream->inq); ++i) {
    buf = nghttp3_ringbuf_get(&stream->inq, i);

    assert_size(nghttp3_min((size_t)NGHTTP3_STREAM_MIN_INQ_BUFLEN << i,
                            (size_t)NGHTTP3_STREAM_CAP_INQ_BUFLEN),
                ==, nghttp3_buf_cap(buf));
    assert_memory_equal(nghttp3_buf_len(buf), &data[nalloc], buf->pos);

    nalloc += nghttp3_buf_cap(buf);
  }

  assert_size(NGHTTP3_STREAM_CAP_INQ_BUFLEN * 2, <=, nalloc);
  assert_size(NGHTTP3_STREAM_CAP_INQ_BUFLEN * 4, >, nalloc);

  nghttp3_stream_del(stream);

  /* The first buffer is large enough to store the data that is
     larger than NGHTTP3_STREAM_MIN_INQ_BUFLEN. */
  rv = nghttp3_stream_new(&stream, 0, NULL, &out_chunk_objalloc,
                          &stream_objalloc, mem);

  assert_int(0, ==, rv);

  rv =
    nghttp3_stream_buffer_data(stream, data, NGHTTP3_STREAM_MIN_INQ_BUFLEN + 1);

  assert_int(0, ==, rv);
  assert_size(1, ==, nghttp3_ringbuf_len(&stream->inq));

  buf = nghttp3_ringbuf_get(&stream->inq, 0);

  assert_size(NGHTTP3_STREAM_MIN_INQ_BUFLEN * 2, ==, nghttp3_buf_cap(buf));
  assert_size(NGHTTP3_STREAM_MIN_INQ_BUFLEN + 1, ==, nghttp3_buf_len(buf));
  assert_memory_equal(nghttp3_buf_len(buf), data, buf->pos);

  nghttp3_stream_del(stream);

  /* Large data > NGHTTP3_STREAM_CAP_INQ_BUFLEN is allocated just once */
  rv = nghttp3_stream_new(&stream, 0, NULL, &out_chunk_objalloc,
                          &stream_objalloc, mem);

  assert_int(0, ==, rv);

  rv = nghttp3_stream_buffer_data(stream, data, sizeof(data));

  assert_int(0, ==, rv);
  assert_size(1, ==, nghttp3_ringbuf_len(&stream->inq));
  assert_size(sizeof(data), ==, nghttp3_stream_get_buffered_datalen(stream));

  buf = nghttp3_ringbuf_get(&stream->inq, 0);

  assert_size(sizeof(data), ==, nghttp3_buf_cap(buf));
  assert_memory_equal(nghttp3_buf_len(buf), &data[0], buf->pos);

  nghttp3_stream_del(stream);

  nghttp3_objalloc_free(&stream_objalloc);
  nghttp3_objalloc_free(&out_chunk_objalloc);
}
