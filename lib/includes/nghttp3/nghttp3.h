/*
 * nghttp3
 *
 * Copyright (c) 2018 nghttp3 contributors
 * Copyright (c) 2017 ngtcp2 contributors
 * Copyright (c) 2017 nghttp2 contributors
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
#ifndef NGHTTP3_H
#define NGHTTP3_H

/* Define WIN32 when build target is Win32 API (borrowed from
   libcurl) */
#if (defined(_WIN32) || defined(__WIN32__)) && !defined(WIN32)
#  define WIN32
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#if defined(_MSC_VER) && (_MSC_VER < 1800)
/* MSVC < 2013 does not have inttypes.h because it is not C99
   compliant.  See compiler macros and version number in
   https://sourceforge.net/p/predef/wiki/Compilers/ */
#  include <stdint.h>
#else /* !defined(_MSC_VER) || (_MSC_VER >= 1800) */
#  include <inttypes.h>
#endif /* !defined(_MSC_VER) || (_MSC_VER >= 1800) */
#include <sys/types.h>
#include <stdarg.h>

#include <nghttp3/version.h>

#ifdef NGHTTP3_STATICLIB
#  define NGHTTP3_EXTERN
#elif defined(WIN32)
#  ifdef BUILDING_NGHTTP3
#    define NGHTTP3_EXTERN __declspec(dllexport)
#  else /* !BUILDING_NGHTTP3 */
#    define NGHTTP3_EXTERN __declspec(dllimport)
#  endif /* !BUILDING_NGHTTP3 */
#else    /* !defined(WIN32) */
#  ifdef BUILDING_NGHTTP3
#    define NGHTTP3_EXTERN __attribute__((visibility("default")))
#  else /* !BUILDING_NGHTTP3 */
#    define NGHTTP3_EXTERN
#  endif /* !BUILDING_NGHTTP3 */
#endif   /* !defined(WIN32) */

typedef enum {
  NGHTTP3_ERR_INVALID_ARGUMENT = -101,
  NGHTTP3_ERR_QPACK_FATAL = -401,
  NGHTTP3_ERR_QPACK_DECOMPRESSION_FAILED = -402,
  NGHTTP3_ERR_QPACK_ENCODER_STREAM = -403,
  NGHTTP3_ERR_QPACK_DECODER_STREAM = -404,
  NGHTTP3_ERR_FATAL = -500,
  NGHTTP3_ERR_NOMEM = -501
} nghttp2_lib_error;

/**
 * @functypedef
 *
 * Custom memory allocator to replace malloc().  The |mem_user_data|
 * is the mem_user_data member of :type:`nghttp3_mem` structure.
 */
typedef void *(*nghttp3_malloc)(size_t size, void *mem_user_data);

/**
 * @functypedef
 *
 * Custom memory allocator to replace free().  The |mem_user_data| is
 * the mem_user_data member of :type:`nghttp3_mem` structure.
 */
typedef void (*nghttp3_free)(void *ptr, void *mem_user_data);

/**
 * @functypedef
 *
 * Custom memory allocator to replace calloc().  The |mem_user_data|
 * is the mem_user_data member of :type:`nghttp3_mem` structure.
 */
typedef void *(*nghttp3_calloc)(size_t nmemb, size_t size, void *mem_user_data);

/**
 * @functypedef
 *
 * Custom memory allocator to replace realloc().  The |mem_user_data|
 * is the mem_user_data member of :type:`nghttp3_mem` structure.
 */
typedef void *(*nghttp3_realloc)(void *ptr, size_t size, void *mem_user_data);

/**
 * @struct
 *
 * Custom memory allocator functions and user defined pointer.  The
 * |mem_user_data| member is passed to each allocator function.  This
 * can be used, for example, to achieve per-session memory pool.
 *
 * In the following example code, ``my_malloc``, ``my_free``,
 * ``my_calloc`` and ``my_realloc`` are the replacement of the
 * standard allocators ``malloc``, ``free``, ``calloc`` and
 * ``realloc`` respectively::
 *
 *     void *my_malloc_cb(size_t size, void *mem_user_data) {
 *       return my_malloc(size);
 *     }
 *
 *     void my_free_cb(void *ptr, void *mem_user_data) { my_free(ptr); }
 *
 *     void *my_calloc_cb(size_t nmemb, size_t size, void *mem_user_data) {
 *       return my_calloc(nmemb, size);
 *     }
 *
 *     void *my_realloc_cb(void *ptr, size_t size, void *mem_user_data) {
 *       return my_realloc(ptr, size);
 *     }
 *
 *     void conn_new() {
 *       nghttp3_mem mem = {NULL, my_malloc_cb, my_free_cb, my_calloc_cb,
 *                          my_realloc_cb};
 *
 *       ...
 *     }
 */
typedef struct {
  /**
   * An arbitrary user supplied data.  This is passed to each
   * allocator function.
   */
  void *mem_user_data;
  /**
   * Custom allocator function to replace malloc().
   */
  nghttp3_malloc malloc;
  /**
   * Custom allocator function to replace free().
   */
  nghttp3_free free;
  /**
   * Custom allocator function to replace calloc().
   */
  nghttp3_calloc calloc;
  /**
   * Custom allocator function to replace realloc().
   */
  nghttp3_realloc realloc;
} nghttp3_mem;

/* The default, system standard memory allocator */
NGHTTP3_EXTERN nghttp3_mem *nghttp3_mem_default(void);

/**
 * @struct
 *
 * nghttp3_vec is struct iovec compatible structure to reference
 * arbitrary array of bytes.
 */
typedef struct {
  /**
   * base points to the data.
   */
  uint8_t *base;
  /**
   * len is the number of bytes which the buffer pointed by base
   * contains.
   */
  size_t len;
} nghttp3_vec;

struct nghttp3_rcbuf;

/**
 * @struct
 *
 * :type:`nghttp3_rcbuf` is the object representing reference counted
 * buffer.  The details of this structure are intentionally hidden
 * from the public API.
 */
typedef struct nghttp3_rcbuf nghttp3_rcbuf;

/**
 * @function
 *
 * `nghttp3_rcbuf_incref` increments the reference count of |rcbuf| by
 * 1.
 */
NGHTTP3_EXTERN void nghttp3_rcbuf_incref(nghttp3_rcbuf *rcbuf);

/**
 * @function
 *
 * `nghttp3_rcbuf_decref` decrements the reference count of |rcbuf| by
 * 1.  If the reference count becomes zero, the object pointed by
 * |rcbuf| will be freed.  In this case, application must not use
 * |rcbuf| again.
 */
NGHTTP3_EXTERN void nghttp3_rcbuf_decref(nghttp3_rcbuf *rcbuf);

/**
 * @function
 *
 * `nghttp3_rcbuf_get_buf` returns the underlying buffer managed by
 * |rcbuf|.
 */
NGHTTP3_EXTERN nghttp3_vec nghttp3_rcbuf_get_buf(nghttp3_rcbuf *rcbuf);

/**
 * @function
 *
 * `nghttp3_rcbuf_is_static` returns nonzero if the underlying buffer
 * is statically allocated, and 0 otherwise. This can be useful for
 * language bindings that wish to avoid creating duplicate strings for
 * these buffers.
 */
NGHTTP3_EXTERN int nghttp3_rcbuf_is_static(const nghttp3_rcbuf *rcbuf);

/**
 * @struct
 *
 * :type:`nghttp3_buf` is the variable size buffer.
 */
typedef struct {
  /**
   * begin points to the beginning of the buffer.
   */
  uint8_t *begin;
  /**
   * end points to the one beyond of the last byte of the buffer
   */
  uint8_t *end;
  /**
   * pos pointers to the start of data.  Typically, this points to the
   * point that next data should be read.  Initially, it points to
   * |begin|.
   */
  uint8_t *pos;
  /**
   * last points to the one beyond of the last data of the buffer.
   * Typically, new data is written at this point.  Initially, it
   * points to |begin|.
   */
  uint8_t *last;
} nghttp3_buf;

/**
 * @function
 *
 * `nghttp3_buf_init` initializes empty |buf|.
 */
NGHTTP3_EXTERN void nghttp3_buf_init(nghttp3_buf *buf);

/**
 * @function
 *
 * `nghttp3_buf_free` frees resources allocated for |buf| using |mem|
 * as memory allocator.  buf->begin must be a heap buffer allocated by
 * |mem|.
 */
NGHTTP3_EXTERN void nghttp3_buf_free(nghttp3_buf *buf, nghttp3_mem *mem);

/**
 * @function
 *
 * `nghttp3_buf_left` returns the number of additional bytes which can
 * be written to the underlying buffer.  In other words, it returns
 * buf->end - buf->last.
 */
NGHTTP3_EXTERN size_t nghttp3_buf_left(const nghttp3_buf *buf);

/**
 * @function
 *
 * `nghttp3_buf_len` returns the number of bytes left to read.  In
 * other words, it returns buf->last - buf->pos.
 */
NGHTTP3_EXTERN size_t nghttp3_buf_len(const nghttp3_buf *buf);

/**
 * @function
 *
 * `nghttp3_buf_reset` sets buf->pos and buf->last to buf->begin.
 */
NGHTTP3_EXTERN void nghttp3_buf_reset(nghttp3_buf *buf);

/**
 * @enum
 *
 * :type:`nghttp3_nv_flag` is the flags for header field name/value
 * pair.
 */
typedef enum {
  /**
   * :enum:`NGHTTP3_NV_FLAG_NONE` indicates no flag set.
   */
  NGHTTP3_NV_FLAG_NONE = 0,
  /**
   * :enum:`NGHTTP3_NV_FLAG_NEVER_INDEX` indicates that this
   * name/value pair must not be indexed.  Other implementation calls
   * this bit as "sensitive".
   */
  NGHTTP3_NV_FLAG_NEVER_INDEX = 0x01,
} nghttp3_nv_flag;

/**
 * @struct
 *
 * :type:`nghttp3_nv` is the name/value pair, which mainly used to
 * represent header fields.
 */
typedef struct {
  /**
   * name is the header field name.
   */
  uint8_t *name;
  /**
   * value is the header field value.
   */
  uint8_t *value;
  /**
   * namelen is the length of the |name|, excluding terminating NULL.
   */
  size_t namelen;
  /**
   * valuelen is the length of the |value|, excluding terminating
   * NULL.
   */
  size_t valuelen;
  /**
   * flags is bitwise OR of one or more of :type:`nghttp3_nv_flag`.
   */
  uint8_t flags;
} nghttp3_nv;

/* Generated by mkstatichdtbl.py */
typedef enum {
  NGHTTP3_QPACK_TOKEN__AUTHORITY = 0,
  NGHTTP3_QPACK_TOKEN__PATH = 8,
  NGHTTP3_QPACK_TOKEN_AGE = 43,
  NGHTTP3_QPACK_TOKEN_CONTENT_DISPOSITION = 52,
  NGHTTP3_QPACK_TOKEN_CONTENT_LENGTH = 55,
  NGHTTP3_QPACK_TOKEN_COOKIE = 68,
  NGHTTP3_QPACK_TOKEN_DATE = 69,
  NGHTTP3_QPACK_TOKEN_ETAG = 71,
  NGHTTP3_QPACK_TOKEN_IF_MODIFIED_SINCE = 74,
  NGHTTP3_QPACK_TOKEN_IF_NONE_MATCH = 75,
  NGHTTP3_QPACK_TOKEN_LAST_MODIFIED = 77,
  NGHTTP3_QPACK_TOKEN_LINK = 78,
  NGHTTP3_QPACK_TOKEN_LOCATION = 79,
  NGHTTP3_QPACK_TOKEN_REFERER = 83,
  NGHTTP3_QPACK_TOKEN_SET_COOKIE = 85,
  NGHTTP3_QPACK_TOKEN__METHOD = 1,
  NGHTTP3_QPACK_TOKEN__SCHEME = 9,
  NGHTTP3_QPACK_TOKEN__STATUS = 11,
  NGHTTP3_QPACK_TOKEN_ACCEPT = 25,
  NGHTTP3_QPACK_TOKEN_ACCEPT_ENCODING = 27,
  NGHTTP3_QPACK_TOKEN_ACCEPT_RANGES = 29,
  NGHTTP3_QPACK_TOKEN_ACCESS_CONTROL_ALLOW_HEADERS = 32,
  NGHTTP3_QPACK_TOKEN_ACCESS_CONTROL_ALLOW_ORIGIN = 38,
  NGHTTP3_QPACK_TOKEN_CACHE_CONTROL = 46,
  NGHTTP3_QPACK_TOKEN_CONTENT_ENCODING = 53,
  NGHTTP3_QPACK_TOKEN_CONTENT_TYPE = 57,
  NGHTTP3_QPACK_TOKEN_RANGE = 82,
  NGHTTP3_QPACK_TOKEN_STRICT_TRANSPORT_SECURITY = 86,
  NGHTTP3_QPACK_TOKEN_VARY = 92,
  NGHTTP3_QPACK_TOKEN_X_CONTENT_TYPE_OPTIONS = 94,
  NGHTTP3_QPACK_TOKEN_X_XSS_PROTECTION = 98,
  NGHTTP3_QPACK_TOKEN_ACCEPT_LANGUAGE = 28,
  NGHTTP3_QPACK_TOKEN_ACCESS_CONTROL_ALLOW_CREDENTIALS = 30,
  NGHTTP3_QPACK_TOKEN_ACCESS_CONTROL_ALLOW_METHODS = 35,
  NGHTTP3_QPACK_TOKEN_ACCESS_CONTROL_EXPOSE_HEADERS = 39,
  NGHTTP3_QPACK_TOKEN_ACCESS_CONTROL_REQUEST_HEADERS = 40,
  NGHTTP3_QPACK_TOKEN_ACCESS_CONTROL_REQUEST_METHOD = 41,
  NGHTTP3_QPACK_TOKEN_ALT_SVC = 44,
  NGHTTP3_QPACK_TOKEN_AUTHORIZATION = 45,
  NGHTTP3_QPACK_TOKEN_CONTENT_SECURITY_POLICY = 56,
  NGHTTP3_QPACK_TOKEN_EARLY_DATA = 70,
  NGHTTP3_QPACK_TOKEN_EXPECT_CT = 72,
  NGHTTP3_QPACK_TOKEN_FORWARDED = 73,
  NGHTTP3_QPACK_TOKEN_IF_RANGE = 76,
  NGHTTP3_QPACK_TOKEN_ORIGIN = 80,
  NGHTTP3_QPACK_TOKEN_PURPOSE = 81,
  NGHTTP3_QPACK_TOKEN_SERVER = 84,
  NGHTTP3_QPACK_TOKEN_TIMING_ALLOW_ORIGIN = 89,
  NGHTTP3_QPACK_TOKEN_UPGRADE_INSECURE_REQUESTS = 90,
  NGHTTP3_QPACK_TOKEN_USER_AGENT = 91,
  NGHTTP3_QPACK_TOKEN_X_FORWARDED_FOR = 95,
  NGHTTP3_QPACK_TOKEN_X_FRAME_OPTIONS = 96
} nghttp3_qpack_token;

/**
 * @struct
 *
 * nghttp3_qpack_nv represents header field name/value pair just like
 * :type:`nghttp3_nv`.  It is an extended version of
 * :type:`nghttp3_nv` and has reference counted buffers and tokens
 * which might be useful for applications.
 */
typedef struct {
  /* The buffer containing header field name.  NULL-termination is
     guaranteed. */
  nghttp3_rcbuf *name;
  /* The buffer containing header field value.  NULL-termination is
     guaranteed. */
  nghttp3_rcbuf *value;
  /* nghttp3_qpack_token value for name.  It could be -1 if we have no
     token for that header field name. */
  int32_t token;
  /* Bitwise OR of one or more of nghttp3_nv_flag. */
  uint8_t flags;
} nghttp3_qpack_nv;

struct nghttp3_qpack_encoder;

/**
 * @struct
 *
 * :type:`nghttp3_qpack_encoder` is QPACK encoder.
 */
typedef struct nghttp3_qpack_encoder nghttp3_qpack_encoder;

/**
 * @function
 *
 * `nghttp3_qpack_encoder_new` initializes QPACK encoder.  |pencoder|
 * must be non-NULL pointer.  |max_dtable_size| is the maximum dynamic
 * table size.  |max_blocked| is the maximum number of streams which
 * can be blocked.  |mem| is a memory allocator.  This function
 * allocates memory for :type:`nghttp3_qpack_encoder` itself and
 * assigns its pointer to |*pencoder| if it succeeds.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :enum:`NGHTTP3_ERR_NOMEM`
 *     Out of memory.
 */
NGHTTP3_EXTERN int nghttp3_qpack_encoder_new(nghttp3_qpack_encoder **pencoder,
                                             size_t max_dtable_size,
                                             size_t max_blocked,
                                             nghttp3_mem *mem);

/**
 * @function
 *
 * `nghttp3_qpack_encoder_del` frees memory allocated for |encoder|.
 * This function frees memory pointed by |encoder| itself.
 */
NGHTTP3_EXTERN void nghttp3_qpack_encoder_del(nghttp3_qpack_encoder *encoder);

/**
 * @function
 *
 * `nghttp3_qpack_encoder_encode` encodes the list of header fields
 * |nva|.  |nvlen| is the length of |nva|.  |stream_id| is the
 * identifier of the stream which this header fields belong to.  This
 * function writes header block prefix, encoded header fields, and
 * encoder stream to |pbuf|, |rbuf|, and |ebuf| respectively.  The
 * last field of nghttp3_buf will be adjusted when data is written.
 * An application should write |pbuf| and |rbuf| to the request stream
 * in this order.
 *
 * The buffer pointed by |pbuf|, |rbuf|, and |ebuf| can be empty
 * buffer.  It is fine to pass a buffer initialized by
 * nghttp3_buf_init(buf).  This function allocates memory for these
 * buffers as necessary.  In particular, it frees and expands buffer
 * if the current capacity of buffer is not enough.  If begin field of
 * any buffer is not NULL, it must be allocated by the same memory
 * allocator passed to `nghttp3_qpack_encoder_new()`.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :enum:`NGHTTP3_ERR_NOMEM`
 *     Out of memory
 * :enum:`NGHTTP3_ERR_QPACK_FATAL`
 *      |encoder| is in unrecoverable error state and cannot be used
 *      anymore.
 */
NGHTTP3_EXTERN int nghttp3_qpack_encoder_encode(
    nghttp3_qpack_encoder *encoder, nghttp3_buf *pbuf, nghttp3_buf *rbuf,
    nghttp3_buf *ebuf, int64_t stream_id, const nghttp3_nv *nva, size_t nvlen);

/**
 * @function
 *
 * `nghttp3_qpack_encoder_read_decoder` reads decoder stream.  The
 * buffer pointed by |src| of length |srclen| contains decoder stream.
 *
 * This function returns the number of bytes read, or one of the
 * following negative error codes:
 *
 * :enum:`NGHTTP3_ERR_NOMEM`
 *     Out of memory
 * :enum:`NGHTTP3_ERR_QPACK_FATAL`
 *     |encoder| is in unrecoverable error state and cannot be used
 *     anymore.
 * :enum:`NGHTTP3_ERR_QPACK_DECODER_STREAM`
 *     |encoder| is unable to process input because it is malformed.
 */
NGHTTP3_EXTERN ssize_t nghttp3_qpack_encoder_read_decoder(
    nghttp3_qpack_encoder *encoder, const uint8_t *src, size_t srclen);

/**
 * @function
 *
 * `nghttp3_qpack_encoder_set_max_dtable_size` sets max dynamic table
 * size to |max_dtable_size|.
 *
 * This function returns the number of bytes read, or one of the
 * following negative error codes:
 *
 * :enum:`NGHTTP3_ERR_INVALID_ARGUMENT`
 *     |max_dtable_size| exceeds the hard limit that decoder specifies.
 */
NGHTTP3_EXTERN int
nghttp3_qpack_encoder_set_max_dtable_size(nghttp3_qpack_encoder *encoder,
                                          size_t max_dtable_size);

/**
 * @function
 *
 * `nghttp3_qpack_encoder_ack_header` tells |encoder| that header
 * block for a stream denoted by |stream_id| was acknowledged by
 * decoder.  This function is provided for debugging purpose only.  In
 * HTTP/3, |encoder| knows acknowledgement of header block by reading
 * decoder stream with `nghttp3_qpack_encoder_read_decoder()`.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :enum:`NGHTTP3_ERR_NOMEM`
 *     Out of memory.
 * :enum:`NGHTTP3_QPACK_DECODER_STREAM`
 *     stream denoted by |stream_id| is not found.
 */
NGHTTP3_EXTERN int
nghttp3_qpack_encoder_ack_header(nghttp3_qpack_encoder *encoder,
                                 int64_t stream_id);

/**
 * @function
 *
 * `nghttp3_qpack_encoder_add_insert_count` increments known received
 * count of |encoder| by |n|.  This function is provided for debugging
 * purpose only.  In HTTP/3, |encoder| increments known received count
 * by reading decoder stream with
 * `nghttp3_qpack_encoder_read_decoder()`.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :enum:`NGHTTP3_ERR_NOMEM`
 *     Out of memory.
 * :enum:`NGHTTP3_QPACK_DECODER_STREAM`
 *     |n| is too large.
 */
NGHTTP3_EXTERN int
nghttp3_qpack_encoder_add_insert_count(nghttp3_qpack_encoder *encoder,
                                       size_t n);

/**
 * @function
 *
 * `nghttp3_qpack_encoder_ack_everything` tells |encoder| that all
 * encoded header blocks are acknowledged.  This function is provided
 * for debugging purpose only.  In HTTP/3, |encoder| knows this by
 * reading decoder stream with `nghttp3_qpack_encoder_read_decoder()`.
 */
NGHTTP3_EXTERN void
nghttp3_qpack_encoder_ack_everything(nghttp3_qpack_encoder *encoder);

/**
 * @function
 *
 * `nghttp3_qpack_encoder_cancel_stream` tells |encoder| that stream
 * denoted by |stream_id| is cancelled.  This function is provided for
 * debugging purpose only.  In HTTP/3, |encoder| knows this by reading
 * decoder stream with `nghttp3_qpack_encoder_read_decoder()`.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :enum:`NGHTTP3_ERR_NOMEM`
 *     Out of memory.
 * :enum:`NGHTTP3_QPACK_DECODER_STREAM`
 *     stream denoted by |stream_id| is not found.
 */
NGHTTP3_EXTERN int
nghttp3_qpack_encoder_cancel_stream(nghttp3_qpack_encoder *encoder,
                                    int64_t stream_id);

/**
 * @function
 *
 * `nghttp3_qpack_encoder_get_num_blocked` returns the number of
 * streams which is potentially blocked at decoder side.
 */
NGHTTP3_EXTERN size_t
nghttp3_qpack_encoder_get_num_blocked(nghttp3_qpack_encoder *encoder);

struct nghttp3_qpack_stream_context;

/**
 * @struct
 *
 * :type:`nghttp3_qpack_stream_context` is a decoder context for an
 * individual stream.
 */
typedef struct nghttp3_qpack_stream_context nghttp3_qpack_stream_context;

/**
 * @function
 *
 * `nghttp3_qpack_stream_context_new` initializes stream context.
 * |psctx| must be non-NULL pointer.  |stream_id| is stream ID.  |mem|
 * is a memory allocator.  This function allocates memory for
 * :type:`nghttp3_qpack_stream_context` itself and assigns its pointer
 * to |*psctx| if it succeeds.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :enum:`NGHTTP3_ERR_NOMEM`
 *     Out of memory.
 */
NGHTTP3_EXTERN int
nghttp3_qpack_stream_context_new(nghttp3_qpack_stream_context **psctx,
                                 int64_t stream_id, nghttp3_mem *mem);

/**
 * @function
 *
 * `nghttp3_qpack_stream_context_del` frees memory allocated for
 * |sctx|.  This function frees memory pointed by |sctx| itself.
 */
NGHTTP3_EXTERN void
nghttp3_qpack_stream_context_del(nghttp3_qpack_stream_context *sctx);

/**
 * @function
 *
 * `nghttp3_qpack_stream_context_get_ricnt` returns required insert
 * count.
 */
NGHTTP3_EXTERN size_t
nghttp3_qpack_stream_context_get_ricnt(nghttp3_qpack_stream_context *sctx);

struct nghttp3_qpack_decoder;

/**
 * @struct
 *
 * `nghttp3_qpack_decoder` is QPACK decoder.
 */
typedef struct nghttp3_qpack_decoder nghttp3_qpack_decoder;

/**
 * @function
 *
 * `nghttp3_qpack_decoder_new` initializes QPACK decoder.  |pdecoder|
 * must be non-NULL pointer.  |max_dtable_size| is the maximum dynamic
 * table size.  |max_blocked| is the maximum number of streams which
 * can be blocked.  |mem| is a memory allocator.  This function
 * allocates memory for :type:`nghttp3_qpack_decoder` itself and
 * assigns its pointer to |*pdecoder| if it succeeds.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :enum:`NGHTTP3_ERR_NOMEM`
 *     Out of memory.
 */
NGHTTP3_EXTERN int nghttp3_qpack_decoder_new(nghttp3_qpack_decoder **pdecoder,
                                             size_t max_dtable_size,
                                             size_t max_blocked,
                                             nghttp3_mem *mem);

/**
 * @function
 *
 * `nghttp3_qpack_decoder_del` frees memory allocated for |decoder|.
 * This function frees memory pointed by |decoder| itself.
 */
NGHTTP3_EXTERN void nghttp3_qpack_decoder_del(nghttp3_qpack_decoder *decoder);

/**
 * @function
 *
 * `nghttp3_qpack_decoder_read_encoder` reads encoder stream.  The
 * buffer pointed by |src| of length |srclen| contains encoder stream.
 *
 * This function returns the number of bytes read, or one of the
 * following negative error codes:
 *
 * TBD
 */
NGHTTP3_EXTERN ssize_t nghttp3_qpack_decoder_read_encoder(
    nghttp3_qpack_decoder *decoder, const uint8_t *src, size_t srclen);

/**
 * @function
 *
 * `nghttp3_qpack_decoder_get_icnt` returns insert count.
 */
NGHTTP3_EXTERN size_t
nghttp3_qpack_decoder_get_icnt(const nghttp3_qpack_decoder *decoder);

/**
 * @enum
 *
 * :type:`nghttp3_qpack_decode_flag` is a set of flags for decoder.
 */
typedef enum {
  /**
   * :enum:`NGHTTP3_QPACK_DECODE_FLAG_NONE` indicates that no flag
   * set.
   */
  NGHTTP3_QPACK_DECODE_FLAG_NONE,
  /**
   * :enum:`NGHTTP3_QPACK_DECODE_FLAG_EMIT` indicates that a header
   * field is successfully decoded.
   */
  NGHTTP3_QPACK_DECODE_FLAG_EMIT = 0x01,
  /**
   * :enum:`NGHTTP3_QPACK_DECODE_FLAG_FINAL` indicates that all header
   * fields have been decoded.
   */
  NGHTTP3_QPACK_DECODE_FLAG_FINAL = 0x02,
  /**
   * :enum:`NGHTTP3_QPACK_DECODE_FLAG_BLOCKED` indicates that decoding
   * has been blocked.
   */
  NGHTTP3_QPACK_DECODE_FLAG_BLOCKED = 0x04
} nghttp3_qpack_decode_flag;

/**
 * @function
 *
 * `nghttp3_qpack_decoder_read_request` reads request stream.  The
 * request stream is given as the buffer pointed by |src| of length
 * |srclen|.  |sctx| is the stream context and it must be initialized
 * by `nghttp3_qpack_stream_context_init()`.  |*pflags| must be
 * non-NULL pointer.  |nv| must be non-NULL pointer.
 *
 * If this function succeeds, it assigns flags to |*pflags|.  If
 * |*pflags| has :enum:`NGHTTP3_QPACK_DECODE_FLAG_EMIT` set, a decoded
 * header field is assigned to |nv|.  If |*pflags| has
 * :enum:`NGHTTP3_QPACK_DECODE_FLAG_FINAL` set, all header fields have
 * been successfully decoded.  If |*pflags| has
 * :enum:`NGHTTP3_QPACK_DECODE_FLAG_BLOCKED` set, decoding is blocked
 * due to required insert count.
 *
 * When a header field is decoded, an application receives it in |nv|.
 * nv->name and nv->value are reference counted buffer, and the their
 * reference counts are already incremented for application use.
 * Therefore, when application finishes processing the header field,
 * it must call nghttp3_rcbuf_decref(nv->name) and
 * nghttp3_rcbuf_decref(nv->value) or memory leak might occur.
 *
 * This function returns the number of bytes read, or one of the
 * following negative error codes:
 *
 * TBD
 */
NGHTTP3_EXTERN ssize_t nghttp3_qpack_decoder_read_request(
    nghttp3_qpack_decoder *decoder, nghttp3_qpack_stream_context *sctx,
    nghttp3_qpack_nv *nv, uint8_t *pflags, const uint8_t *src, size_t srclen,
    int fin);

/**
 * @function
 *
 * `nghttp3_qpack_decoder_write_decoder` writes decoder stream into
 * |dbuf|.
 *
 * The buffer pointed by |dbuf| can be empty buffer.  It is fine to
 * pass a buffer initialized by nghttp3_buf_init(buf).  This function
 * allocates memory for these buffers as necessary.  In particular, it
 * frees and expands buffer if the current capacity of buffer is not
 * enough.  If begin field of any buffer is not NULL, it must be
 * allocated by the same memory allocator passed to
 * `nghttp3_qpack_encoder_new()`.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * :enum:`NGHTTP3_ERR_NOMEM`
 *     Out of memory
 * TBD
 */
NGHTTP3_EXTERN int
nghttp3_qpack_decoder_write_decoder(nghttp3_qpack_decoder *decoder,
                                    nghttp3_buf *dbuf);

/**
 * @function
 *
 * `nghttp3_qpack_decoder_cancel_stream` cancels header decoding for
 * stream denoted by |stream_id|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * TBD
 */
NGHTTP3_EXTERN int
nghttp3_qpack_decoder_cancel_stream(nghttp3_qpack_decoder *decoder,
                                    int64_t stream_id);

/**
 * @function
 *
 * `nghttp3_strerror` returns textual representation of |liberr| which
 * should be one of error codes defined in :type:`nghttp3_lib_error`.
 */
NGHTTP3_EXTERN const char *nghttp3_strerror(int liberr);

/**
 * @functypedef
 *
 * :type:`nghttp3_debug_vprintf_callback` is a callback function
 * invoked when the library outputs debug logging.  The function is
 * called with arguments suitable for ``vfprintf(3)``
 *
 * The debug output is only enabled if the library is built with
 * ``DEBUGBUILD`` macro defined.
 */
typedef void (*nghttp3_debug_vprintf_callback)(const char *format,
                                               va_list args);

/**
 * @function
 *
 * `nghttp3_set_debug_vprintf_callback` sets a debug output callback
 * called by the library when built with ``DEBUGBUILD`` macro defined.
 * If this option is not used, debug log is written into standard
 * error output.
 *
 * For builds without ``DEBUGBUILD`` macro defined, this function is
 * noop.
 *
 * Note that building with ``DEBUGBUILD`` may cause significant
 * performance penalty to libnghttp3 because of extra processing.  It
 * should be used for debugging purpose only.
 *
 * .. Warning::
 *
 *   Building with ``DEBUGBUILD`` may cause significant performance
 *   penalty to libnghttp3 because of extra processing.  It should be
 *   used for debugging purpose only.  We write this two times because
 *   this is important.
 */
NGHTTP3_EXTERN void nghttp3_set_debug_vprintf_callback(
    nghttp3_debug_vprintf_callback debug_vprintf_callback);

#ifdef __cplusplus
}
#endif

#endif /* NGHTTP3_H */
