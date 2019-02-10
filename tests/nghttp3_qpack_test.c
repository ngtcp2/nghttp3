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
#include "nghttp3_qpack_test.h"

#include <CUnit/CUnit.h>

#include "nghttp3_qpack.h"
#include "nghttp3_macro.h"

#define MAKE_NV(NAME, VALUE)                                                   \
  {                                                                            \
    (uint8_t *)(NAME), (uint8_t *)(VALUE), sizeof((NAME)) - 1,                 \
        sizeof((VALUE)) - 1, NGHTTP3_NV_FLAG_NONE                              \
  }

static void check_decode_header(nghttp3_qpack_decoder *dec, nghttp3_buf *pbuf,
                                nghttp3_buf *rbuf, nghttp3_buf *ebuf,
                                const nghttp3_nv *nva, size_t nvlen,
                                nghttp3_mem *mem) {
  ssize_t nread;
  nghttp3_qpack_stream_context sctx;
  nghttp3_qpack_nv qnv;
  const nghttp3_nv *nv;
  uint8_t flags;
  size_t i = 0;

  nread =
      nghttp3_qpack_decoder_read_encoder(dec, ebuf->pos, nghttp3_buf_len(ebuf));

  CU_ASSERT((ssize_t)nghttp3_buf_len(ebuf) == nread);

  nghttp3_qpack_stream_context_init(&sctx, mem);

  nread = nghttp3_qpack_decoder_read_request(
      dec, &sctx, &qnv, &flags, pbuf->pos, nghttp3_buf_len(pbuf), 0);

  CU_ASSERT((ssize_t)nghttp3_buf_len(pbuf) == nread);

  for (; nghttp3_buf_len(rbuf);) {
    nread = nghttp3_qpack_decoder_read_request(
        dec, &sctx, &qnv, &flags, rbuf->pos, nghttp3_buf_len(rbuf), 1);

    CU_ASSERT(nread > 0);

    if (nread < 0) {
      break;
    }

    rbuf->pos += nread;

    if (flags & NGHTTP3_QPACK_DECODE_FLAG_FINAL) {
      break;
    }
    if (flags & NGHTTP3_QPACK_DECODE_FLAG_EMIT) {
      nv = &nva[i++];
      CU_ASSERT(nv->namelen == qnv.name->len);
      CU_ASSERT(0 == memcmp(nv->name, qnv.name->base, nv->namelen));
      CU_ASSERT(nv->valuelen == qnv.value->len);
      CU_ASSERT(0 == memcmp(nv->value, qnv.value->base, nv->valuelen));

      nghttp3_rcbuf_decref(qnv.name);
      nghttp3_rcbuf_decref(qnv.value);
    }
  }

  CU_ASSERT(i == nvlen);

  nghttp3_qpack_stream_context_free(&sctx);
  nghttp3_buf_reset(pbuf);
  nghttp3_buf_reset(rbuf);
  nghttp3_buf_reset(ebuf);
}

void test_nghttp3_qpack_encoder_encode(void) {
  nghttp3_mem *mem = nghttp3_mem_default();
  nghttp3_qpack_encoder enc;
  nghttp3_qpack_decoder dec;
  nghttp3_nv nva[] = {
      MAKE_NV(":path", "/rsrc.php/v3/yn/r/rIPZ9Qkrdd9.png"),
      MAKE_NV(":authority", "static.xx.fbcdn.net"),
      MAKE_NV(":scheme", "https"),
      MAKE_NV(":method", "GET"),
      MAKE_NV("accept-encoding", "gzip, deflate, br"),
      MAKE_NV("accept-language", "en-US,en;q=0.9"),
      MAKE_NV(
          "user-agent",
          "Mozilla/5.0 (Windows NT 10.0; Win64; x64)AppleWebKit/537.36(KHTML, "
          "like Gecko) Chrome/63.0.3239.70 Safari/537.36"),
      MAKE_NV("accept", "image/webp,image/apng,image/*,*/*;q=0.8"),
      MAKE_NV("referer", "https://static.xx.fbcdn.net/rsrc.php/v3/yT/l/0,cross/"
                         "dzXGESIlGQQ.css"),
  };
  int rv;
  nghttp3_buf pbuf, rbuf, ebuf;
  nghttp3_qpack_stream *stream;
  nghttp3_qpack_entry_ref *ref;

  nghttp3_buf_init(&pbuf);
  nghttp3_buf_init(&rbuf);
  nghttp3_buf_init(&ebuf);
  rv = nghttp3_qpack_encoder_init(&enc, 4096, 1, mem);

  CU_ASSERT(0 == rv);

  rv = nghttp3_qpack_decoder_init(&dec, 4096, 1, mem);

  CU_ASSERT(0 == rv);

  rv = nghttp3_qpack_encoder_encode(&enc, &pbuf, &rbuf, &ebuf, 0, nva,
                                    nghttp3_arraylen(nva));

  CU_ASSERT(0 == rv);

  stream = nghttp3_qpack_encoder_find_stream(&enc, 0);

  CU_ASSERT(NULL != stream);
  CU_ASSERT(nghttp3_qpack_encoder_stream_is_blocked(&enc, stream));
  CU_ASSERT(1 == nghttp3_qpack_encoder_get_num_blocked(&enc));

  ref = stream->ref;

  CU_ASSERT(NULL != ref);
  CU_ASSERT(5 == ref->max_cnt);
  CU_ASSERT(1 == ref->min_cnt);
  CU_ASSERT(5 == stream->max_cnt);
  CU_ASSERT(1 == stream->min_cnt);
  CU_ASSERT(2 == nghttp3_buf_len(&pbuf));

  check_decode_header(&dec, &pbuf, &rbuf, &ebuf, nva, nghttp3_arraylen(nva),
                      mem);

  rv = nghttp3_qpack_encoder_encode(&enc, &pbuf, &rbuf, &ebuf, 4, nva,
                                    nghttp3_arraylen(nva));

  CU_ASSERT(0 == rv);

  stream = nghttp3_qpack_encoder_find_stream(&enc, 4);

  CU_ASSERT(NULL == stream);

  check_decode_header(&dec, &pbuf, &rbuf, &ebuf, nva, nghttp3_arraylen(nva),
                      mem);

  nghttp3_qpack_encoder_ack_header(&enc, 0);

  CU_ASSERT(5 == enc.krcnt);

  rv = nghttp3_qpack_encoder_encode(&enc, &pbuf, &rbuf, &ebuf, 8, nva,
                                    nghttp3_arraylen(nva));

  CU_ASSERT(0 == rv);
  CU_ASSERT(0 == nghttp3_qpack_encoder_get_num_blocked(&enc));

  check_decode_header(&dec, &pbuf, &rbuf, &ebuf, nva, nghttp3_arraylen(nva),
                      mem);

  nghttp3_qpack_decoder_free(&dec);
  nghttp3_qpack_encoder_free(&enc);
  nghttp3_buf_free(&ebuf, mem);
  nghttp3_buf_free(&rbuf, mem);
  nghttp3_buf_free(&pbuf, mem);
}

void test_nghttp3_qpack_encoder_still_blocked(void) {
  nghttp3_mem *mem = nghttp3_mem_default();
  nghttp3_qpack_encoder enc;
  nghttp3_nv nva1[] = {
      MAKE_NV(":status", "103"),
      MAKE_NV("link", "foo"),
  };
  nghttp3_nv nva2[] = {
      MAKE_NV(":status", "200"),
      MAKE_NV("content-type", "text/foo"),
  };
  int rv;
  nghttp3_buf pbuf, rbuf, ebuf;
  nghttp3_qpack_stream *stream;
  /* nghttp3_qpack_entry_ref *ref; */

  nghttp3_buf_init(&pbuf);
  nghttp3_buf_init(&rbuf);
  nghttp3_buf_init(&ebuf);
  rv = nghttp3_qpack_encoder_init(&enc, 4096, 1, mem);

  CU_ASSERT(0 == rv);

  rv = nghttp3_qpack_encoder_encode(&enc, &pbuf, &rbuf, &ebuf, 0, nva1,
                                    nghttp3_arraylen(nva1));

  CU_ASSERT(0 == rv);
  CU_ASSERT(1 == nghttp3_qpack_encoder_get_num_blocked(&enc));

  rv = nghttp3_qpack_encoder_encode(&enc, &pbuf, &rbuf, &ebuf, 0, nva2,
                                    nghttp3_arraylen(nva2));

  CU_ASSERT(0 == rv);
  CU_ASSERT(1 == nghttp3_qpack_encoder_get_num_blocked(&enc));

  stream = nghttp3_qpack_encoder_find_stream(&enc, 0);

  CU_ASSERT(NULL != stream->ref->next);
  CU_ASSERT(stream->ref->max_cnt != stream->max_cnt);
  CU_ASSERT(stream->ref->next->max_cnt == stream->max_cnt);

  rv = nghttp3_qpack_encoder_ack_header(&enc, 0);

  CU_ASSERT(0 == rv);
  CU_ASSERT(1 == nghttp3_qpack_encoder_get_num_blocked(&enc));

  stream = nghttp3_qpack_encoder_find_stream(&enc, 0);

  CU_ASSERT(NULL != stream->ref);
  CU_ASSERT(NULL == stream->ref->next);
  CU_ASSERT(stream->ref->max_cnt == stream->max_cnt);

  rv = nghttp3_qpack_encoder_ack_header(&enc, 0);

  CU_ASSERT(0 == rv);
  CU_ASSERT(0 == nghttp3_qpack_encoder_get_num_blocked(&enc));
  CU_ASSERT(NULL == nghttp3_qpack_encoder_find_stream(&enc, 0));

  nghttp3_qpack_encoder_free(&enc);
  nghttp3_buf_free(&ebuf, mem);
  nghttp3_buf_free(&rbuf, mem);
  nghttp3_buf_free(&pbuf, mem);
}
