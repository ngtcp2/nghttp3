/*
 * nghttp3
 *
 * Copyright (c) 2019 nghttp3 contributors
 * Copyright (c) 2013 nghttp2 contributors
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
#ifndef NGHTTP3_QPACK_H
#define NGHTTP3_QPACK_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <nghttp3/nghttp3.h>

#include "nghttp3_rcbuf.h"
#include "nghttp3_map.h"
#include "nghttp3_pq.h"
#include "nghttp3_ringbuf.h"
#include "nghttp3_buf.h"
#include "nghttp3_ksl.h"
#include "nghttp3_qpack_huffman.h"

#define NGHTTP3_QPACK_INT_MAX ((1ull << 62) - 1)

typedef enum {
  NGHTTP3_QPACK_INDEXING_MODE_LITERAL,
  NGHTTP3_QPACK_INDEXING_MODE_STORE,
  NGHTTP3_QPACK_INDEXING_MODE_NEVER,
} nghttp3_qpack_indexing_mode;

struct nghttp3_qpack_entry;
typedef struct nghttp3_qpack_entry nghttp3_qpack_entry;

struct nghttp3_qpack_entry {
  /* The header field name/value pair */
  nghttp3_qpack_nv nv;
  /* map_next points to the entry which shares same bucket in hash
     table. */
  nghttp3_qpack_entry *map_next;
  /* sum is the sum of all entries inserted up to this entry.  This
     value does not contain the space required for this entry. */
  size_t sum;
  /* absidx is the absolute index of this entry. */
  size_t absidx;
  /* The hash value for header name (nv.name). */
  uint32_t hash;
};

/* The entry used for static table. */
typedef struct {
  /* TODO We don't need name */
  nghttp3_rcbuf name;
  /* TODO We don't need rcbuf here */
  nghttp3_rcbuf value;
  size_t absidx;
  int32_t token;
  uint32_t hash;
} nghttp3_qpack_static_entry;

typedef struct {
  nghttp3_rcbuf name;
  nghttp3_rcbuf value;
  int32_t token;
} nghttp3_qpack_static_header;

struct nghttp3_qpack_entry_ref;
typedef struct nghttp3_qpack_entry_ref nghttp3_qpack_entry_ref;

struct nghttp3_qpack_entry_ref {
  nghttp3_qpack_entry_ref *next;
  size_t max_cnt;
  size_t min_cnt;
};

void nghttp3_qpack_entry_ref_init(nghttp3_qpack_entry_ref *ref, size_t max_cnt,
                                  size_t min_cnt);

typedef struct {
  nghttp3_pq_entry pe;
  nghttp3_map_entry me;
  nghttp3_qpack_entry_ref *ref;
  size_t max_cnt;
  size_t min_cnt;
} nghttp3_qpack_stream;

void nghttp3_qpack_stream_init(nghttp3_qpack_stream *stream, int64_t stream_id);

void nghttp3_qpack_stream_free(nghttp3_qpack_stream *stream, nghttp3_mem *mem);

void nghttp3_qpack_stream_add_ref(nghttp3_qpack_stream *stream,
                                  nghttp3_qpack_entry_ref *ref);

void nghttp3_qpack_stream_pop_ref(nghttp3_qpack_stream *stream,
                                  nghttp3_mem *mem);

#define NGHTTP3_QPACK_ENTRY_OVERHEAD 32

typedef struct {
  /* dtable is a dynamic table */
  nghttp3_ringbuf dtable;
  /* mem is memory allocator */
  nghttp3_mem *mem;
  /* dtable_size is abstracted buffer size of dtable as described in
     the spec. This is the sum of length of name/value in dtable +
     NGHTTP3_QPACK_ENTRY_OVERHEAD bytes overhead per each entry. */
  size_t dtable_size;
  size_t dtable_sum;
  /* dtable_size is the effective maximum size of dynamic table. */
  size_t max_dtable_size;
  /* next_absidx is the next absolute index for nghttp3_qpack_entry */
  size_t next_absidx;
  /* If inflate/deflate error occurred, this value is set to 1 and
     further invocation of inflate/deflate will fail with
     NGHTTP3_ERR_QPACK_FATAL. */
  uint8_t bad;
} nghttp3_qpack_context;

#define NGHTTP3_QPACK_MAP_SIZE 128

typedef struct {
  nghttp3_qpack_entry *table[NGHTTP3_QPACK_MAP_SIZE];
} nghttp3_qpack_map;

struct nghttp3_qpack_encoder {
  nghttp3_qpack_context ctx;
  nghttp3_qpack_map dtable_map;
  nghttp3_map stream_refs;
  nghttp3_ksl blocked_refs;
  nghttp3_pq refsq;
  size_t max_blocked;
  size_t krcnt;
};

int nghttp3_qpack_encoder_init(nghttp3_qpack_encoder *encoder,
                               size_t max_dtable_size, size_t max_blocked,
                               nghttp3_mem *mem);

void nghttp3_qpack_encoder_free(nghttp3_qpack_encoder *encoder);

int nghttp3_qpack_encoder_encode_nv(nghttp3_qpack_encoder *encoder,
                                    size_t *pmax_cnt, size_t *pmin_cnt,
                                    nghttp3_buf *rbuf, nghttp3_buf *ebuf,
                                    const nghttp3_nv *nv, size_t base,
                                    int allow_blocking);

typedef struct {
  ssize_t index;
  /* Nonzero if both name and value are matched. */
  int name_value_match;
  /* pb_index is the absolute index of matched post-based dynamic
     table entry. */
  ssize_t pb_index;
} nghttp3_qpack_lookup_result;

nghttp3_qpack_lookup_result
nghttp3_qpack_lookup_stable(const nghttp3_nv *nv, int32_t token,
                            nghttp3_qpack_indexing_mode indexing_mode);

nghttp3_qpack_lookup_result
nghttp3_qpack_lookup_dtable(nghttp3_qpack_map *dtable_map, const nghttp3_nv *nv,
                            int32_t token,
                            nghttp3_qpack_indexing_mode indexing_mode,
                            uint32_t hash, size_t krcnt, int allow_blocking);

int nghttp3_qpack_encoder_write_header_block_prefix(
    nghttp3_qpack_encoder *encoder, nghttp3_buf *pbuf, size_t max_cnt,
    size_t base);

int nghttp3_qpack_encoder_write_static_indexed(nghttp3_qpack_encoder *encoder,
                                               nghttp3_buf *rbuf,
                                               size_t absidx);

int nghttp3_qpack_encoder_write_dynamic_indexed(nghttp3_qpack_encoder *encoder,
                                                nghttp3_buf *rbuf,
                                                size_t absidx, size_t base);

int nghttp3_qpack_encoder_write_static_indexed_name(
    nghttp3_qpack_encoder *encoder, nghttp3_buf *rbuf, size_t absidx,
    const nghttp3_nv *nv);

int nghttp3_qpack_encoder_write_dynamic_indexed_name(
    nghttp3_qpack_encoder *encoder, nghttp3_buf *rbuf, size_t absidx,
    size_t base, const nghttp3_nv *nv);

int nghttp3_qpack_encoder_write_literal(nghttp3_qpack_encoder *encoder,
                                        nghttp3_buf *rbuf,
                                        const nghttp3_nv *nv);

int nghttp3_qpack_encoder_write_static_insert(nghttp3_qpack_encoder *encoder,
                                              nghttp3_buf *ebuf, size_t absidx,
                                              const nghttp3_nv *nv);

int nghttp3_qpack_encoder_write_dynamic_insert(nghttp3_qpack_encoder *encoder,
                                               nghttp3_buf *ebuf, size_t absidx,
                                               const nghttp3_nv *nv);

int nghttp3_qpack_encoder_write_duplicate_insert(nghttp3_qpack_encoder *encoder,
                                                 nghttp3_buf *ebuf,
                                                 size_t absidx);

int nghttp3_qpack_encoder_write_literal_insert(nghttp3_qpack_encoder *encoder,
                                               nghttp3_buf *ebuf,
                                               const nghttp3_nv *nv);

int nghttp3_qpack_encoder_stream_is_blocked(nghttp3_qpack_encoder *encoder,
                                            nghttp3_qpack_stream *stream);

int nghttp3_qpack_encoder_block_stream(nghttp3_qpack_encoder *encoder,
                                       nghttp3_qpack_stream *stream);

int nghttp3_qpack_encoder_unblock_stream(nghttp3_qpack_encoder *encoder,
                                         size_t max_cnt);

nghttp3_qpack_stream *
nghttp3_qpack_encoder_find_stream(nghttp3_qpack_encoder *encoder,
                                  int64_t stream_id);

int nghttp3_qpack_context_dtable_set_dtable_cap(nghttp3_qpack_context *ctx,
                                                size_t cap);

int nghttp3_qpack_context_dtable_add(nghttp3_qpack_context *ctx,
                                     nghttp3_qpack_nv *qnv,
                                     nghttp3_qpack_map *dtable_map,
                                     uint32_t hash);

int nghttp3_qpack_encoder_dtable_static_add(nghttp3_qpack_encoder *encoder,
                                            size_t absidx, const nghttp3_nv *nv,
                                            uint32_t hash);

int nghttp3_qpack_encoder_dtable_dynamic_add(nghttp3_qpack_encoder *encoder,
                                             size_t absidx,
                                             const nghttp3_nv *nv,
                                             uint32_t hash);

int nghttp3_qpack_encoder_dtable_duplicate_add(nghttp3_qpack_encoder *encoder,
                                               size_t absidx);

int nghttp3_qpack_encoder_dtable_literal_add(nghttp3_qpack_encoder *encoder,
                                             const nghttp3_nv *nv,
                                             int32_t token, uint32_t hash);

nghttp3_qpack_entry *
nghttp3_qpack_context_dtable_get(nghttp3_qpack_context *ctx, size_t absidx);

nghttp3_qpack_entry *
nghttp3_qpack_context_dtable_top(nghttp3_qpack_context *ctx);

void nghttp3_qpack_entry_init(nghttp3_qpack_entry *ent, nghttp3_qpack_nv *qnv,
                              size_t sum, size_t absidx, uint32_t hash);

void nghttp3_qpack_entry_free(nghttp3_qpack_entry *ent);

size_t nghttp3_qpack_put_varint_len(uint64_t n, size_t prefix);

uint8_t *nghttp3_qpack_put_varint(uint8_t *buf, uint64_t n, size_t prefix);

typedef enum {
  NGHTTP3_QPACK_ES_STATE_OPCODE,
  NGHTTP3_QPACK_ES_STATE_READ_INDEX,
  NGHTTP3_QPACK_ES_STATE_CHECK_NAME_HUFFMAN,
  NGHTTP3_QPACK_ES_STATE_READ_NAMELEN,
  NGHTTP3_QPACK_ES_STATE_READ_NAME_HUFFMAN,
  NGHTTP3_QPACK_ES_STATE_READ_NAME,
  NGHTTP3_QPACK_ES_STATE_CHECK_VALUE_HUFFMAN,
  NGHTTP3_QPACK_ES_STATE_READ_VALUELEN,
  NGHTTP3_QPACK_ES_STATE_READ_VALUE_HUFFMAN,
  NGHTTP3_QPACK_ES_STATE_READ_VALUE,
} nghttp3_qpack_encoder_stream_state;

typedef enum {
  NGHTTP3_QPACK_ES_OPCODE_INSERT_INDEXED,
  NGHTTP3_QPACK_ES_OPCODE_INSERT,
  NGHTTP3_QPACK_ES_OPCODE_DUPLICATE,
  NGHTTP3_QPACK_ES_OPCODE_SET_DTABLE_CAP,
} nghttp3_qpack_encoder_stream_opcode;

typedef enum {
  NGHTTP3_QPACK_RS_STATE_RICNT,
  NGHTTP3_QPACK_RS_STATE_DBASE_SIGN,
  NGHTTP3_QPACK_RS_STATE_DBASE,
  NGHTTP3_QPACK_RS_STATE_OPCODE,
  NGHTTP3_QPACK_RS_STATE_READ_INDEX,
  NGHTTP3_QPACK_RS_STATE_CHECK_NAME_HUFFMAN,
  NGHTTP3_QPACK_RS_STATE_READ_NAMELEN,
  NGHTTP3_QPACK_RS_STATE_READ_NAME_HUFFMAN,
  NGHTTP3_QPACK_RS_STATE_READ_NAME,
  NGHTTP3_QPACK_RS_STATE_CHECK_VALUE_HUFFMAN,
  NGHTTP3_QPACK_RS_STATE_READ_VALUELEN,
  NGHTTP3_QPACK_RS_STATE_READ_VALUE_HUFFMAN,
  NGHTTP3_QPACK_RS_STATE_READ_VALUE,
  NGHTTP3_QPACK_RS_STATE_BLOCKED,
} nghttp3_qpack_request_stream_state;

typedef enum {
  NGHTTP3_QPACK_RS_OPCODE_INDEXED,
  NGHTTP3_QPACK_RS_OPCODE_INDEXED_PB,
  NGHTTP3_QPACK_RS_OPCODE_INDEXED_NAME,
  NGHTTP3_QPACK_RS_OPCODE_INDEXED_NAME_PB,
  NGHTTP3_QPACK_RS_OPCODE_LITERAL,
} nghttp3_qpack_request_stream_opcode;

typedef struct {
  nghttp3_qpack_huffman_decode_context huffman_ctx;
  nghttp3_buf namebuf;
  nghttp3_buf valuebuf;
  nghttp3_rcbuf *name;
  nghttp3_rcbuf *value;
  uint64_t left;
  size_t prefix;
  size_t shift;
  size_t absidx;
  int never;
  int dynamic;
  int huffman_encoded;
} nghttp3_qpack_read_state;

void nghttp3_qpack_read_state_free(nghttp3_qpack_read_state *rstate);

void nghttp3_qpack_read_state_reset(nghttp3_qpack_read_state *rstate);

struct nghttp3_qpack_decoder {
  nghttp3_qpack_context ctx;
  size_t max_blocked;
  nghttp3_qpack_encoder_stream_state state;
  nghttp3_qpack_encoder_stream_opcode opcode;
  nghttp3_qpack_read_state rstate;
};

int nghttp3_qpack_decoder_init(nghttp3_qpack_decoder *decoder,
                               size_t max_dtable_size, size_t max_blocked,
                               nghttp3_mem *mem);

void nghttp3_qpack_decoder_free(nghttp3_qpack_decoder *decoder);

int nghttp3_qpack_decoder_dtable_indexed_add(nghttp3_qpack_decoder *decoder);

int nghttp3_qpack_decoder_dtable_static_add(nghttp3_qpack_decoder *decoder);

int nghttp3_qpack_decoder_dtable_dynamic_add(nghttp3_qpack_decoder *decoder);

int nghttp3_qpack_decoder_dtable_duplicate_add(nghttp3_qpack_decoder *decoder);

int nghttp3_qpack_decoder_dtable_literal_add(nghttp3_qpack_decoder *decoder);

struct nghttp3_qpack_stream_context {
  nghttp3_qpack_request_stream_state state;
  nghttp3_qpack_read_state rstate;
  nghttp3_mem *mem;
  nghttp3_qpack_request_stream_opcode opcode;
  size_t ricnt;
  size_t base;
  int dbase_sign;
};

void nghttp3_qpack_stream_context_init(nghttp3_qpack_stream_context *sctx,
                                       nghttp3_mem *mem);

void nghttp3_qpack_stream_context_free(nghttp3_qpack_stream_context *sctx);

int nghttp3_qpack_decoder_compute_ricnt(nghttp3_qpack_decoder *decoder,
                                        size_t *dest, size_t encricnt);

int nghttp3_qpack_decoder_rel2abs(nghttp3_qpack_decoder *decoder,
                                  nghttp3_qpack_read_state *rstate);

int nghttp3_qpack_decoder_brel2abs(nghttp3_qpack_decoder *decoder,
                                   nghttp3_qpack_stream_context *sctx);

int nghttp3_qpack_decoder_pbrel2abs(nghttp3_qpack_decoder *decoder,
                                    nghttp3_qpack_stream_context *sctx);

void nghttp3_qpack_decoder_emit_indexed(nghttp3_qpack_decoder *decoder,
                                        nghttp3_qpack_stream_context *sctx,
                                        nghttp3_qpack_nv *nv);

void nghttp3_qpack_decoder_emit_indexed_name(nghttp3_qpack_decoder *decoder,
                                             nghttp3_qpack_stream_context *sctx,
                                             nghttp3_qpack_nv *nv);

void nghttp3_qpack_decoder_emit_literal(nghttp3_qpack_decoder *decoder,
                                        nghttp3_qpack_stream_context *sctx,
                                        nghttp3_qpack_nv *nv);

#endif /* NGHTTP3_QPACK_H */
