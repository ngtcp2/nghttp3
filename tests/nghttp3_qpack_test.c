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

#include <stdio.h>
#include <stdlib.h>

#include <CUnit/CUnit.h>

#include "nghttp3_qpack.h"
#include "nghttp3_macro.h"
#include "nghttp3_test_helper.h"

static void check_decode_header(nghttp3_qpack_decoder *dec, nghttp3_buf *pbuf,
                                nghttp3_buf *rbuf, nghttp3_buf *ebuf,
                                int64_t stream_id, const nghttp3_nv *nva,
                                size_t nvlen, const nghttp3_mem *mem) {
  nghttp3_ssize nread;
  nghttp3_qpack_stream_context sctx;
  nghttp3_qpack_nv qnv;
  const nghttp3_nv *nv;
  uint8_t flags;
  size_t i = 0;

  nread =
      nghttp3_qpack_decoder_read_encoder(dec, ebuf->pos, nghttp3_buf_len(ebuf));

  CU_ASSERT((nghttp3_ssize)nghttp3_buf_len(ebuf) == nread);

  nghttp3_qpack_stream_context_init(&sctx, stream_id, mem);

  nread = nghttp3_qpack_decoder_read_request(
      dec, &sctx, &qnv, &flags, pbuf->pos, nghttp3_buf_len(pbuf), 0);

  CU_ASSERT((nghttp3_ssize)nghttp3_buf_len(pbuf) == nread);

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

static void decode_header_block(nghttp3_qpack_decoder *dec, nghttp3_buf *pbuf,
                                nghttp3_buf *rbuf, int64_t stream_id,
                                const nghttp3_mem *mem) {
  nghttp3_ssize nread;
  nghttp3_qpack_stream_context sctx;
  nghttp3_qpack_nv qnv;
  uint8_t flags;

  nghttp3_qpack_stream_context_init(&sctx, stream_id, mem);

  nread = nghttp3_qpack_decoder_read_request(
      dec, &sctx, &qnv, &flags, pbuf->pos, nghttp3_buf_len(pbuf), 0);

  CU_ASSERT((nghttp3_ssize)nghttp3_buf_len(pbuf) == nread);

  for (;;) {
    nread = nghttp3_qpack_decoder_read_request(
        dec, &sctx, &qnv, &flags, rbuf->pos, nghttp3_buf_len(rbuf), 1);

    CU_ASSERT(nread >= 0);

    if (nread < 0) {
      break;
    }

    if (flags & NGHTTP3_QPACK_DECODE_FLAG_FINAL) {
      CU_ASSERT(nread == 0);
      CU_ASSERT(!(flags & NGHTTP3_QPACK_DECODE_FLAG_EMIT));
      CU_ASSERT(0 == nghttp3_buf_len(rbuf));
      CU_ASSERT(nghttp3_buf_len(&dec->dbuf) > 0);

      break;
    }

    CU_ASSERT(nread > 0);
    CU_ASSERT(flags & NGHTTP3_QPACK_DECODE_FLAG_EMIT);

    nghttp3_rcbuf_decref(qnv.name);
    nghttp3_rcbuf_decref(qnv.value);

    rbuf->pos += nread;
  }

  nghttp3_qpack_stream_context_free(&sctx);
}

void test_nghttp3_qpack_encoder_encode(void) {
  const nghttp3_mem *mem = nghttp3_mem_default();
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
  nghttp3_qpack_header_block_ref *ref;

  nghttp3_buf_init(&pbuf);
  nghttp3_buf_init(&rbuf);
  nghttp3_buf_init(&ebuf);
  rv = nghttp3_qpack_encoder_init(&enc, 4096, mem);

  CU_ASSERT(0 == rv);

  nghttp3_qpack_encoder_set_max_blocked_streams(&enc, 1);

  nghttp3_qpack_encoder_set_max_dtable_capacity(&enc, 4096);

  rv = nghttp3_qpack_decoder_init(&dec, 4096, 1, mem);

  CU_ASSERT(0 == rv);

  rv = nghttp3_qpack_encoder_encode(&enc, &pbuf, &rbuf, &ebuf, 0, nva,
                                    nghttp3_arraylen(nva));

  CU_ASSERT(0 == rv);

  stream = nghttp3_qpack_encoder_find_stream(&enc, 0);

  CU_ASSERT(NULL != stream);
  CU_ASSERT(nghttp3_qpack_encoder_stream_is_blocked(&enc, stream));
  CU_ASSERT(1 == nghttp3_qpack_encoder_get_num_blocked_streams(&enc));

  ref =
      *(nghttp3_qpack_header_block_ref **)nghttp3_ringbuf_get(&stream->refs, 0);

  CU_ASSERT(5 == ref->max_cnt);
  CU_ASSERT(1 == ref->min_cnt);
  CU_ASSERT(5 == nghttp3_qpack_stream_get_max_cnt(stream));
  CU_ASSERT(1 == nghttp3_qpack_encoder_get_min_cnt(&enc));
  CU_ASSERT(2 == nghttp3_buf_len(&pbuf));

  check_decode_header(&dec, &pbuf, &rbuf, &ebuf, 0, nva, nghttp3_arraylen(nva),
                      mem);

  rv = nghttp3_qpack_encoder_encode(&enc, &pbuf, &rbuf, &ebuf, 4, nva,
                                    nghttp3_arraylen(nva));

  CU_ASSERT(0 == rv);

  stream = nghttp3_qpack_encoder_find_stream(&enc, 4);

  CU_ASSERT(NULL == stream);

  check_decode_header(&dec, &pbuf, &rbuf, &ebuf, 4, nva, nghttp3_arraylen(nva),
                      mem);

  nghttp3_qpack_encoder_ack_header(&enc, 0);

  CU_ASSERT(5 == enc.krcnt);

  rv = nghttp3_qpack_encoder_encode(&enc, &pbuf, &rbuf, &ebuf, 8, nva,
                                    nghttp3_arraylen(nva));

  CU_ASSERT(0 == rv);
  CU_ASSERT(0 == nghttp3_qpack_encoder_get_num_blocked_streams(&enc));

  check_decode_header(&dec, &pbuf, &rbuf, &ebuf, 8, nva, nghttp3_arraylen(nva),
                      mem);

  nghttp3_qpack_decoder_free(&dec);
  nghttp3_qpack_encoder_free(&enc);
  nghttp3_buf_free(&ebuf, mem);
  nghttp3_buf_free(&rbuf, mem);
  nghttp3_buf_free(&pbuf, mem);
}

void test_nghttp3_qpack_encoder_encode_try_encode(void) {
  const nghttp3_mem *mem = nghttp3_mem_default();
  nghttp3_qpack_encoder enc;
  nghttp3_nv nva[] = {
      MAKE_NV(":path", "/foo"),
      MAKE_NV(":authority", "example.com"),
      MAKE_NV("authorization", "bearer token"),
      MAKE_NV("priority", "i"),
      MAKE_NV("cookie", "short00000000000000"),
      MAKE_NV("cookie", "large000000000000000"),
      MAKE_NV("nonstd", "non-standard-cookie"),
  };
  int rv;
  nghttp3_buf pbuf, rbuf, ebuf;
  nghttp3_qpack_entry *ent;
  size_t i;

  /* NGHTTP3_NV_FLAG_TRY_INDEX indexes fields which are normally not
     indexed. */
  nghttp3_buf_init(&pbuf);
  nghttp3_buf_init(&rbuf);
  nghttp3_buf_init(&ebuf);
  rv = nghttp3_qpack_encoder_init(&enc, 4096, mem);

  CU_ASSERT(0 == rv);

  nghttp3_qpack_encoder_set_max_blocked_streams(&enc, 1);

  nghttp3_qpack_encoder_set_max_dtable_capacity(&enc, 4096);

  rv = nghttp3_qpack_encoder_encode(&enc, &pbuf, &rbuf, &ebuf, 0, nva,
                                    nghttp3_arraylen(nva));

  CU_ASSERT(0 == rv);
  CU_ASSERT(3 == nghttp3_ringbuf_len(&enc.ctx.dtable));

  ent = *(nghttp3_qpack_entry **)nghttp3_ringbuf_get(&enc.ctx.dtable, 0);

  CU_ASSERT(nva[5].namelen == ent->nv.name->len);
  CU_ASSERT(0 == memcmp(nva[5].name, ent->nv.name->base, ent->nv.name->len));

  ent = *(nghttp3_qpack_entry **)nghttp3_ringbuf_get(&enc.ctx.dtable, 1);

  CU_ASSERT(nva[3].namelen == ent->nv.name->len);
  CU_ASSERT(0 == memcmp(nva[3].name, ent->nv.name->base, ent->nv.name->len));

  ent = *(nghttp3_qpack_entry **)nghttp3_ringbuf_get(&enc.ctx.dtable, 2);

  CU_ASSERT(nva[1].namelen == ent->nv.name->len);
  CU_ASSERT(0 == memcmp(nva[1].name, ent->nv.name->base, ent->nv.name->len));

  nghttp3_qpack_encoder_free(&enc);
  nghttp3_buf_reset(&ebuf);
  nghttp3_buf_reset(&rbuf);
  nghttp3_buf_reset(&pbuf);

  for (i = 0; i < nghttp3_arraylen(nva); ++i) {
    nva[i].flags = NGHTTP3_NV_FLAG_TRY_INDEX;
  }

  rv = nghttp3_qpack_encoder_init(&enc, 4096, mem);

  CU_ASSERT(0 == rv);

  nghttp3_qpack_encoder_set_max_blocked_streams(&enc, 1);

  nghttp3_qpack_encoder_set_max_dtable_capacity(&enc, 4096);

  rv = nghttp3_qpack_encoder_encode(&enc, &pbuf, &rbuf, &ebuf, 0, nva,
                                    nghttp3_arraylen(nva));

  CU_ASSERT(0 == rv);
  CU_ASSERT(5 == nghttp3_ringbuf_len(&enc.ctx.dtable));

  ent = *(nghttp3_qpack_entry **)nghttp3_ringbuf_get(&enc.ctx.dtable, 0);

  CU_ASSERT(nva[6].namelen == ent->nv.name->len);
  CU_ASSERT(0 == memcmp(nva[6].name, ent->nv.name->base, ent->nv.name->len));

  ent = *(nghttp3_qpack_entry **)nghttp3_ringbuf_get(&enc.ctx.dtable, 1);

  CU_ASSERT(nva[5].namelen == ent->nv.name->len);
  CU_ASSERT(0 == memcmp(nva[5].name, ent->nv.name->base, ent->nv.name->len));

  ent = *(nghttp3_qpack_entry **)nghttp3_ringbuf_get(&enc.ctx.dtable, 2);

  CU_ASSERT(nva[3].namelen == ent->nv.name->len);
  CU_ASSERT(0 == memcmp(nva[3].name, ent->nv.name->base, ent->nv.name->len));

  ent = *(nghttp3_qpack_entry **)nghttp3_ringbuf_get(&enc.ctx.dtable, 3);

  CU_ASSERT(nva[1].namelen == ent->nv.name->len);
  CU_ASSERT(0 == memcmp(nva[1].name, ent->nv.name->base, ent->nv.name->len));

  ent = *(nghttp3_qpack_entry **)nghttp3_ringbuf_get(&enc.ctx.dtable, 4);

  CU_ASSERT(nva[0].namelen == ent->nv.name->len);
  CU_ASSERT(0 == memcmp(nva[0].name, ent->nv.name->base, ent->nv.name->len));

  nghttp3_qpack_encoder_free(&enc);
  nghttp3_buf_free(&ebuf, mem);
  nghttp3_buf_free(&rbuf, mem);
  nghttp3_buf_free(&pbuf, mem);
}

void test_nghttp3_qpack_encoder_still_blocked(void) {
  const nghttp3_mem *mem = nghttp3_mem_default();
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
  nghttp3_qpack_header_block_ref *ref;

  nghttp3_buf_init(&pbuf);
  nghttp3_buf_init(&rbuf);
  nghttp3_buf_init(&ebuf);
  rv = nghttp3_qpack_encoder_init(&enc, 4096, mem);

  CU_ASSERT(0 == rv);

  nghttp3_qpack_encoder_set_max_blocked_streams(&enc, 1);

  nghttp3_qpack_encoder_set_max_dtable_capacity(&enc, 4096);

  rv = nghttp3_qpack_encoder_encode(&enc, &pbuf, &rbuf, &ebuf, 0, nva1,
                                    nghttp3_arraylen(nva1));

  CU_ASSERT(0 == rv);
  CU_ASSERT(1 == nghttp3_qpack_encoder_get_num_blocked_streams(&enc));

  rv = nghttp3_qpack_encoder_encode(&enc, &pbuf, &rbuf, &ebuf, 0, nva2,
                                    nghttp3_arraylen(nva2));

  CU_ASSERT(0 == rv);
  CU_ASSERT(1 == nghttp3_qpack_encoder_get_num_blocked_streams(&enc));

  stream = nghttp3_qpack_encoder_find_stream(&enc, 0);

  ref =
      *(nghttp3_qpack_header_block_ref **)nghttp3_ringbuf_get(&stream->refs, 0);

  CU_ASSERT(nghttp3_ringbuf_len(&stream->refs) > 1);
  CU_ASSERT(ref->max_cnt != nghttp3_qpack_stream_get_max_cnt(stream));

  ref =
      *(nghttp3_qpack_header_block_ref **)nghttp3_ringbuf_get(&stream->refs, 1);

  CU_ASSERT(ref->max_cnt == nghttp3_qpack_stream_get_max_cnt(stream));

  nghttp3_qpack_encoder_ack_header(&enc, 0);

  CU_ASSERT(1 == nghttp3_qpack_encoder_get_num_blocked_streams(&enc));

  stream = nghttp3_qpack_encoder_find_stream(&enc, 0);

  ref =
      *(nghttp3_qpack_header_block_ref **)nghttp3_ringbuf_get(&stream->refs, 0);

  CU_ASSERT(1 == nghttp3_ringbuf_len(&stream->refs));
  CU_ASSERT(ref->max_cnt == nghttp3_qpack_stream_get_max_cnt(stream));

  nghttp3_qpack_encoder_ack_header(&enc, 0);

  CU_ASSERT(0 == nghttp3_qpack_encoder_get_num_blocked_streams(&enc));
  CU_ASSERT(NULL == nghttp3_qpack_encoder_find_stream(&enc, 0));

  nghttp3_qpack_encoder_free(&enc);
  nghttp3_buf_free(&ebuf, mem);
  nghttp3_buf_free(&rbuf, mem);
  nghttp3_buf_free(&pbuf, mem);
}

void test_nghttp3_qpack_encoder_set_dtable_cap(void) {
  const nghttp3_mem *mem = nghttp3_mem_default();
  nghttp3_qpack_encoder enc;
  nghttp3_qpack_decoder dec;
  nghttp3_buf pbuf, rbuf, ebuf;
  const nghttp3_nv nva1[] = {
      MAKE_NV(":path", "/"),
      MAKE_NV("date", "bar1"),
  };
  const nghttp3_nv nva2[] = {
      MAKE_NV(":path", "/"),
      MAKE_NV("vary", "bar2"),
  };
  int rv;
  nghttp3_ssize nread;

  nghttp3_buf_init(&pbuf);
  nghttp3_buf_init(&rbuf);
  nghttp3_buf_init(&ebuf);

  rv = nghttp3_qpack_encoder_init(&enc, 4096, mem);

  CU_ASSERT(0 == rv);

  nghttp3_qpack_encoder_set_max_blocked_streams(&enc, 3);

  nghttp3_qpack_encoder_set_max_dtable_capacity(&enc, 4096);

  rv = nghttp3_qpack_decoder_init(&dec, 4096, 3, mem);

  CU_ASSERT(0 == rv);

  rv = nghttp3_qpack_encoder_encode(&enc, &pbuf, &rbuf, &ebuf, 0, nva1,
                                    nghttp3_arraylen(nva1));

  CU_ASSERT(0 == rv);
  CU_ASSERT(1 == enc.ctx.next_absidx);
  CU_ASSERT(strlen("date") + strlen("bar1") + NGHTTP3_QPACK_ENTRY_OVERHEAD ==
            enc.ctx.dtable_size);

  nread = nghttp3_qpack_decoder_read_encoder(&dec, ebuf.pos,
                                             nghttp3_buf_len(&ebuf));

  CU_ASSERT(nread == (nghttp3_ssize)nghttp3_buf_len(&ebuf));
  CU_ASSERT(1 == dec.ctx.next_absidx);
  CU_ASSERT(strlen("date") + strlen("bar1") + NGHTTP3_QPACK_ENTRY_OVERHEAD ==
            dec.ctx.dtable_size);
  CU_ASSERT(4096 == dec.ctx.max_dtable_capacity);

  decode_header_block(&dec, &pbuf, &rbuf, 0, mem);
  nghttp3_buf_reset(&pbuf);
  nghttp3_buf_reset(&rbuf);
  nghttp3_buf_reset(&ebuf);

  rv = nghttp3_qpack_encoder_encode(&enc, &pbuf, &rbuf, &ebuf, 4, nva2,
                                    nghttp3_arraylen(nva2));

  CU_ASSERT(0 == rv);
  CU_ASSERT(strlen("date") + strlen("bar1") + NGHTTP3_QPACK_ENTRY_OVERHEAD +
                strlen("vary") + strlen("bar2") +
                NGHTTP3_QPACK_ENTRY_OVERHEAD ==
            enc.ctx.dtable_size);
  CU_ASSERT(2 == enc.ctx.next_absidx);
  CU_ASSERT(2 == nghttp3_qpack_encoder_get_num_blocked_streams(&enc));

  nread = nghttp3_qpack_decoder_read_encoder(&dec, ebuf.pos,
                                             nghttp3_buf_len(&ebuf));

  CU_ASSERT(nread == (nghttp3_ssize)nghttp3_buf_len(&ebuf));
  CU_ASSERT(2 == dec.ctx.next_absidx);
  CU_ASSERT(strlen("date") + strlen("bar1") + NGHTTP3_QPACK_ENTRY_OVERHEAD +
                strlen("vary") + strlen("bar2") +
                NGHTTP3_QPACK_ENTRY_OVERHEAD ==
            dec.ctx.dtable_size);
  CU_ASSERT(4096 == dec.ctx.max_dtable_capacity);

  decode_header_block(&dec, &pbuf, &rbuf, 4, mem);
  nghttp3_buf_reset(&pbuf);
  nghttp3_buf_reset(&rbuf);
  nghttp3_buf_reset(&ebuf);

  nghttp3_qpack_encoder_set_max_dtable_capacity(&enc, 0);

  CU_ASSERT(0 == enc.ctx.max_dtable_capacity);
  CU_ASSERT(2 == nghttp3_qpack_encoder_get_num_blocked_streams(&enc));

  /* Cannot index more headers because we set max_dtable_capacity to
     0. */
  rv = nghttp3_qpack_encoder_encode(&enc, &pbuf, &rbuf, &ebuf, 8, nva2,
                                    nghttp3_arraylen(nva2));

  CU_ASSERT(0 == rv);
  CU_ASSERT(0 == enc.ctx.max_dtable_capacity);
  CU_ASSERT(2 == enc.ctx.next_absidx);
  CU_ASSERT(2 == nghttp3_qpack_encoder_get_num_blocked_streams(&enc));

  nread = nghttp3_qpack_decoder_read_encoder(&dec, ebuf.pos,
                                             nghttp3_buf_len(&ebuf));

  CU_ASSERT(nread == (nghttp3_ssize)nghttp3_buf_len(&ebuf));
  CU_ASSERT(2 == dec.ctx.next_absidx);
  CU_ASSERT(strlen("date") + strlen("bar1") + NGHTTP3_QPACK_ENTRY_OVERHEAD +
                strlen("vary") + strlen("bar2") +
                NGHTTP3_QPACK_ENTRY_OVERHEAD ==
            dec.ctx.dtable_size);
  CU_ASSERT(4096 == dec.ctx.max_dtable_capacity);

  decode_header_block(&dec, &pbuf, &rbuf, 8, mem);
  nghttp3_buf_reset(&pbuf);
  nghttp3_buf_reset(&rbuf);
  nghttp3_buf_reset(&ebuf);

  /* Acking stream 0 will evict first entry */
  nghttp3_qpack_encoder_ack_header(&enc, 0);

  rv = nghttp3_qpack_encoder_encode(&enc, &pbuf, &rbuf, &ebuf, 12, nva2,
                                    nghttp3_arraylen(nva2));

  CU_ASSERT(0 == rv);
  CU_ASSERT(strlen("vary") + strlen("bar2") + NGHTTP3_QPACK_ENTRY_OVERHEAD ==
            enc.ctx.dtable_size);
  CU_ASSERT(0 == enc.ctx.max_dtable_capacity);
  CU_ASSERT(1 == nghttp3_qpack_encoder_get_num_blocked_streams(&enc));

  nread = nghttp3_qpack_decoder_read_encoder(&dec, ebuf.pos,
                                             nghttp3_buf_len(&ebuf));

  CU_ASSERT(nread == (nghttp3_ssize)nghttp3_buf_len(&ebuf));
  /* decoder still has 2 entries because encoder does not emit Set
     Dynamic Table Capacity. */
  CU_ASSERT(2 == dec.ctx.next_absidx);
  CU_ASSERT(strlen("date") + strlen("bar1") + NGHTTP3_QPACK_ENTRY_OVERHEAD +
                strlen("vary") + strlen("bar2") +
                NGHTTP3_QPACK_ENTRY_OVERHEAD ==
            dec.ctx.dtable_size);
  CU_ASSERT(4096 == dec.ctx.max_dtable_capacity);

  decode_header_block(&dec, &pbuf, &rbuf, 12, mem);
  nghttp3_buf_reset(&pbuf);
  nghttp3_buf_reset(&rbuf);
  nghttp3_buf_reset(&ebuf);

  /* Acking stream 4 will evict another entry */
  nghttp3_qpack_encoder_ack_header(&enc, 4);

  rv = nghttp3_qpack_encoder_encode(&enc, &pbuf, &rbuf, &ebuf, 16, nva2,
                                    nghttp3_arraylen(nva2));

  CU_ASSERT(0 == rv);
  CU_ASSERT(0 == enc.ctx.dtable_size);
  CU_ASSERT(0 == enc.ctx.max_dtable_capacity);
  CU_ASSERT(0 == enc.last_max_dtable_update);
  CU_ASSERT(SIZE_MAX == enc.min_dtable_update);
  CU_ASSERT(0 == nghttp3_qpack_encoder_get_num_blocked_streams(&enc));

  nread = nghttp3_qpack_decoder_read_encoder(&dec, ebuf.pos,
                                             nghttp3_buf_len(&ebuf));

  CU_ASSERT(nread == (nghttp3_ssize)nghttp3_buf_len(&ebuf));
  CU_ASSERT(2 == dec.ctx.next_absidx);
  CU_ASSERT(0 == dec.ctx.dtable_size);
  CU_ASSERT(0 == dec.ctx.max_dtable_capacity);

  decode_header_block(&dec, &pbuf, &rbuf, 16, mem);
  nghttp3_buf_reset(&pbuf);
  nghttp3_buf_reset(&rbuf);
  nghttp3_buf_reset(&ebuf);

  nghttp3_qpack_decoder_free(&dec);
  nghttp3_qpack_encoder_free(&enc);

  /* Check that minimum size is emitted */
  nghttp3_qpack_encoder_init(&enc, 4096, mem);
  nghttp3_qpack_encoder_set_max_blocked_streams(&enc, 1);
  nghttp3_qpack_encoder_set_max_dtable_capacity(&enc, 4096);
  nghttp3_qpack_decoder_init(&dec, 4096, 1, mem);

  rv = nghttp3_qpack_encoder_encode(&enc, &pbuf, &rbuf, &ebuf, 0, nva1,
                                    nghttp3_arraylen(nva1));

  CU_ASSERT(0 == rv);
  CU_ASSERT(1 == enc.ctx.next_absidx);
  CU_ASSERT(strlen("date") + strlen("bar1") + NGHTTP3_QPACK_ENTRY_OVERHEAD ==
            enc.ctx.dtable_size);

  nread = nghttp3_qpack_decoder_read_encoder(&dec, ebuf.pos,
                                             nghttp3_buf_len(&ebuf));

  CU_ASSERT(nread == (nghttp3_ssize)nghttp3_buf_len(&ebuf));
  CU_ASSERT(1 == dec.ctx.next_absidx);
  CU_ASSERT(strlen("date") + strlen("bar1") + NGHTTP3_QPACK_ENTRY_OVERHEAD ==
            dec.ctx.dtable_size);
  CU_ASSERT(4096 == dec.ctx.max_dtable_capacity);

  decode_header_block(&dec, &pbuf, &rbuf, 0, mem);
  nghttp3_buf_reset(&pbuf);
  nghttp3_buf_reset(&rbuf);
  nghttp3_buf_reset(&ebuf);

  nghttp3_qpack_encoder_set_max_dtable_capacity(&enc, 0);
  nghttp3_qpack_encoder_set_max_dtable_capacity(&enc, 1024);

  CU_ASSERT(0 == enc.min_dtable_update);
  CU_ASSERT(1024 == enc.last_max_dtable_update);

  nghttp3_qpack_encoder_ack_header(&enc, 0);

  rv = nghttp3_qpack_encoder_encode(&enc, &pbuf, &rbuf, &ebuf, 4, nva1,
                                    nghttp3_arraylen(nva1));

  CU_ASSERT(0 == rv);
  CU_ASSERT(2 == enc.ctx.next_absidx);
  CU_ASSERT(strlen("date") + strlen("bar1") + NGHTTP3_QPACK_ENTRY_OVERHEAD ==
            enc.ctx.dtable_size);
  CU_ASSERT(SIZE_MAX == enc.min_dtable_update);
  CU_ASSERT(1024 == enc.last_max_dtable_update);
  CU_ASSERT(1024 == enc.ctx.max_dtable_capacity);

  nread = nghttp3_qpack_decoder_read_encoder(&dec, ebuf.pos,
                                             nghttp3_buf_len(&ebuf));

  CU_ASSERT(nread == (nghttp3_ssize)nghttp3_buf_len(&ebuf));
  CU_ASSERT(2 == dec.ctx.next_absidx);
  CU_ASSERT(strlen("date") + strlen("bar1") + NGHTTP3_QPACK_ENTRY_OVERHEAD ==
            dec.ctx.dtable_size);
  CU_ASSERT(1024 == dec.ctx.max_dtable_capacity);

  decode_header_block(&dec, &pbuf, &rbuf, 4, mem);
  nghttp3_buf_reset(&pbuf);
  nghttp3_buf_reset(&rbuf);
  nghttp3_buf_reset(&ebuf);

  nghttp3_qpack_decoder_free(&dec);
  nghttp3_qpack_encoder_free(&enc);
  nghttp3_buf_free(&ebuf, mem);
  nghttp3_buf_free(&rbuf, mem);
  nghttp3_buf_free(&pbuf, mem);
}

void test_nghttp3_qpack_decoder_feedback(void) {
  const nghttp3_mem *mem = nghttp3_mem_default();
  nghttp3_qpack_encoder enc;
  nghttp3_qpack_decoder dec;
  nghttp3_buf pbuf1, rbuf1, pbuf2, rbuf2, pbuf3, rbuf3, ebuf, dbuf;
  const nghttp3_nv nva1[] = {
      MAKE_NV(":path", "/"),
      MAKE_NV("date", "bar1"),
  };
  const nghttp3_nv nva2[] = {
      MAKE_NV(":path", "/"),
      MAKE_NV("vary", "bar2"),
  };
  const nghttp3_nv nva3[] = {
      MAKE_NV(":path", "/"),
      MAKE_NV("link", "bar3"),
  };
  int rv;
  nghttp3_ssize nread;

  nghttp3_buf_init(&pbuf1);
  nghttp3_buf_init(&rbuf1);
  nghttp3_buf_init(&pbuf2);
  nghttp3_buf_init(&rbuf2);
  nghttp3_buf_init(&pbuf3);
  nghttp3_buf_init(&rbuf3);
  nghttp3_buf_init(&ebuf);
  nghttp3_buf_init(&dbuf);

  nghttp3_buf_reserve(&dbuf, 4096, mem);

  rv = nghttp3_qpack_encoder_init(&enc, 4096, mem);

  CU_ASSERT(0 == rv);

  nghttp3_qpack_encoder_set_max_blocked_streams(&enc, 2);

  nghttp3_qpack_encoder_set_max_dtable_capacity(&enc, 4096);

  rv = nghttp3_qpack_decoder_init(&dec, 4096, 2, mem);

  CU_ASSERT(0 == rv);

  rv = nghttp3_qpack_encoder_encode(&enc, &pbuf1, &rbuf1, &ebuf, 0, nva1,
                                    nghttp3_arraylen(nva1));

  CU_ASSERT(0 == rv);
  CU_ASSERT(1 == nghttp3_qpack_encoder_get_num_blocked_streams(&enc));

  rv = nghttp3_qpack_encoder_encode(&enc, &pbuf2, &rbuf2, &ebuf, 4, nva2,
                                    nghttp3_arraylen(nva2));

  CU_ASSERT(0 == rv);
  CU_ASSERT(2 == nghttp3_qpack_encoder_get_num_blocked_streams(&enc));

  nread = nghttp3_qpack_decoder_read_encoder(&dec, ebuf.pos,
                                             nghttp3_buf_len(&ebuf));

  CU_ASSERT((nghttp3_ssize)nghttp3_buf_len(&ebuf) == nread);

  /* Process stream 4 first */
  decode_header_block(&dec, &pbuf2, &rbuf2, 4, mem);

  CU_ASSERT(2 == dec.written_icnt);

  nghttp3_qpack_decoder_write_decoder(&dec, &dbuf);

  nread = nghttp3_qpack_encoder_read_decoder(&enc, dbuf.pos,
                                             nghttp3_buf_len(&dbuf));

  CU_ASSERT((nghttp3_ssize)nghttp3_buf_len(&dbuf) == nread);
  /* This will unblock all streams because higher insert count is
     acknowledged. */
  CU_ASSERT(0 == nghttp3_qpack_encoder_get_num_blocked_streams(&enc));
  CU_ASSERT(1 == nghttp3_map_size(&enc.streams));
  CU_ASSERT(1 == nghttp3_pq_size(&enc.min_cnts));
  CU_ASSERT(0 == nghttp3_ksl_len(&enc.blocked_streams));
  CU_ASSERT(2 == enc.krcnt);

  /* Process stream 0 */
  decode_header_block(&dec, &pbuf1, &rbuf1, 0, mem);
  nghttp3_buf_reset(&dbuf);
  nghttp3_qpack_decoder_write_decoder(&dec, &dbuf);

  nread = nghttp3_qpack_encoder_read_decoder(&enc, dbuf.pos,
                                             nghttp3_buf_len(&dbuf));

  CU_ASSERT((nghttp3_ssize)nghttp3_buf_len(&dbuf) == nread);
  CU_ASSERT(0 == nghttp3_map_size(&enc.streams));
  CU_ASSERT(0 == nghttp3_pq_size(&enc.min_cnts));
  CU_ASSERT(2 == enc.krcnt);

  /* Encode another header, and then read encoder stream only.  Write
     decoder stream.  */
  nghttp3_buf_reset(&ebuf);
  rv = nghttp3_qpack_encoder_encode(&enc, &pbuf3, &rbuf3, &ebuf, 8, nva3,
                                    nghttp3_arraylen(nva3));

  CU_ASSERT(0 == rv);
  CU_ASSERT(1 == nghttp3_qpack_encoder_get_num_blocked_streams(&enc));

  nread = nghttp3_qpack_decoder_read_encoder(&dec, ebuf.pos,
                                             nghttp3_buf_len(&ebuf));

  CU_ASSERT((nghttp3_ssize)nghttp3_buf_len(&ebuf) == nread);

  nghttp3_buf_reset(&dbuf);
  nghttp3_qpack_decoder_write_decoder(&dec, &dbuf);

  CU_ASSERT(nghttp3_buf_len(&dbuf) > 0);
  CU_ASSERT(3 == dec.written_icnt);

  nread = nghttp3_qpack_encoder_read_decoder(&enc, dbuf.pos,
                                             nghttp3_buf_len(&dbuf));

  CU_ASSERT((nghttp3_ssize)nghttp3_buf_len(&dbuf) == nread);
  CU_ASSERT(0 == nghttp3_qpack_encoder_get_num_blocked_streams(&enc));
  CU_ASSERT(1 == nghttp3_map_size(&enc.streams));
  CU_ASSERT(3 == enc.krcnt);

  /* Cancel stream 8 */
  rv = nghttp3_qpack_decoder_cancel_stream(&dec, 8);

  CU_ASSERT(0 == rv);

  nghttp3_buf_reset(&dbuf);
  nghttp3_qpack_decoder_write_decoder(&dec, &dbuf);

  CU_ASSERT(nghttp3_buf_len(&dbuf) > 0);

  nread = nghttp3_qpack_encoder_read_decoder(&enc, dbuf.pos,
                                             nghttp3_buf_len(&dbuf));

  CU_ASSERT((nghttp3_ssize)nghttp3_buf_len(&dbuf) == nread);
  CU_ASSERT(0 == nghttp3_qpack_encoder_get_num_blocked_streams(&enc));
  CU_ASSERT(0 == nghttp3_ksl_len(&enc.blocked_streams));
  CU_ASSERT(0 == nghttp3_pq_size(&enc.min_cnts));
  CU_ASSERT(0 == nghttp3_map_size(&enc.streams));

  nghttp3_qpack_decoder_free(&dec);
  nghttp3_qpack_encoder_free(&enc);
  nghttp3_buf_free(&dbuf, mem);
  nghttp3_buf_free(&ebuf, mem);
  nghttp3_buf_free(&rbuf3, mem);
  nghttp3_buf_free(&pbuf3, mem);
  nghttp3_buf_free(&rbuf2, mem);
  nghttp3_buf_free(&pbuf2, mem);
  nghttp3_buf_free(&rbuf1, mem);
  nghttp3_buf_free(&pbuf1, mem);
}

void test_nghttp3_qpack_decoder_stream_overflow(void) {
  const nghttp3_mem *mem = nghttp3_mem_default();
  nghttp3_qpack_decoder dec;
  size_t i;
  int rv;

  nghttp3_qpack_decoder_init(&dec, 4096, 0, mem);

  for (i = 0;; ++i) {
    rv = nghttp3_qpack_decoder_cancel_stream(&dec, (int64_t)i);
    if (rv == NGHTTP3_ERR_QPACK_FATAL) {
      break;
    }
  }

  nghttp3_qpack_decoder_free(&dec);
}

void test_nghttp3_qpack_huffman(void) {
  size_t i, j;
  uint8_t raw[100], ebuf[4096], dbuf[4096];
  uint8_t *end;
  nghttp3_qpack_huffman_decode_context ctx;
  nghttp3_ssize nwrite;

  srand(1000000007);

  for (i = 0; i < 100000; ++i) {
    for (j = 0; j < sizeof(raw); ++j) {
      raw[j] = (uint8_t)((double)rand() / RAND_MAX * 255);
    }
    end = nghttp3_qpack_huffman_encode(ebuf, raw, sizeof(raw));

    nghttp3_qpack_huffman_decode_context_init(&ctx);
    nwrite =
        nghttp3_qpack_huffman_decode(&ctx, dbuf, ebuf, (size_t)(end - ebuf), 1);
    if (nwrite <= 0) {
      CU_ASSERT(nwrite > 0);
      continue;
    }
    CU_ASSERT((sizeof(raw) == (size_t)nwrite));
    CU_ASSERT(0 == memcmp(raw, dbuf, sizeof(raw)));
  }
}

void test_nghttp3_qpack_huffman_decode_failure_state(void) {
  nghttp3_qpack_huffman_decode_context ctx;
  const uint8_t data[] = {0xff, 0xff, 0xff, 0xff};
  uint8_t buf[4096];
  nghttp3_ssize nwrite;

  nghttp3_qpack_huffman_decode_context_init(&ctx);
  nwrite = nghttp3_qpack_huffman_decode(&ctx, buf, data, sizeof(data) - 1, 0);

  CU_ASSERT(0 == nwrite);
  CU_ASSERT(!nghttp3_qpack_huffman_decode_failure_state(&ctx));

  nwrite = nghttp3_qpack_huffman_decode(&ctx, buf, data, 1, 0);

  CU_ASSERT(0 == nwrite);
  CU_ASSERT(nghttp3_qpack_huffman_decode_failure_state(&ctx));
}

void test_nghttp3_qpack_decoder_reconstruct_ricnt(void) {
  const nghttp3_mem *mem = nghttp3_mem_default();
  nghttp3_qpack_decoder dec;
  uint64_t ricnt;
  int rv;

  rv = nghttp3_qpack_decoder_init(&dec, 100, 1, mem);

  CU_ASSERT(0 == rv);

  dec.ctx.next_absidx = 10;

  rv = nghttp3_qpack_decoder_reconstruct_ricnt(&dec, &ricnt, 3);

  CU_ASSERT(0 == rv);
  CU_ASSERT(8 == ricnt);

  nghttp3_qpack_decoder_free(&dec);
}
