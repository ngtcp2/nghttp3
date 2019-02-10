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
#include "nghttp3_qpack_huffman.h"

#include <string.h>
#include <assert.h>
#include <stdio.h>

/*
 * Encodes huffman code |sym| into |*dest_ptr|, whose least |rembits|
 * bits are not filled yet.  The |rembits| must be in range [1, 8],
 * inclusive.  At the end of the process, the |*dest_ptr| is updated
 * and points where next output should be placed. The number of
 * unfilled bits in the pointed location is returned.
 */
static uint8_t *huffman_encode_sym(uint8_t *dest, size_t *prembits,
                                   const nghttp3_qpack_huffman_sym *sym) {
  size_t nbits = sym->nbits;
  size_t rembits = *prembits;
  uint32_t code = sym->code;

  /* We assume that sym->nbits <= 32 */
  if (rembits > nbits) {
    *dest |= (uint8_t)(code << (rembits - nbits));
    *prembits = rembits - nbits;
    return dest;
  }

  if (rembits == nbits) {
    *dest++ |= (uint8_t)code;
    *prembits = 8;
    return dest;
  }

  *dest++ |= (uint8_t)(code >> (nbits - rembits));

  nbits -= rembits;
  if (nbits & 0x7) {
    /* align code to MSB byte boundary */
    code <<= 8 - (nbits & 0x7);
  }

  /* fast path, since most code is less than 8 */
  if (nbits < 8) {
    *dest = (uint8_t)code;
    *prembits = 8 - nbits;
    return dest;
  }

  /* handle longer code path */
  if (nbits > 24) {
    *dest++ = (uint8_t)(code >> 24);
    nbits -= 8;
  }

  if (nbits > 16) {
    *dest++ = (uint8_t)(code >> 16);
    nbits -= 8;
  }

  if (nbits > 8) {
    *dest++ = (uint8_t)(code >> 8);
    nbits -= 8;
  }

  if (nbits == 8) {
    *dest++ = (uint8_t)code;
    *prembits = 8;
    return dest;
  }

  *dest = (uint8_t)code;
  *prembits = 8 - nbits;
  return dest;
}

size_t nghttp3_qpack_huffman_encode_count(const uint8_t *src, size_t len) {
  size_t i;
  size_t nbits = 0;

  for (i = 0; i < len; ++i) {
    nbits += huffman_sym_table[src[i]].nbits;
  }
  /* pad the prefix of EOS (256) */
  return (nbits + 7) / 8;
}

uint8_t *nghttp3_qpack_huffman_encode(uint8_t *dest, const uint8_t *src,
                                      size_t srclen) {
  size_t rembits = 8;
  size_t i;
  const nghttp3_qpack_huffman_sym *sym;

  for (i = 0; i < srclen; ++i) {
    sym = &huffman_sym_table[src[i]];
    if (rembits == 8) {
      *dest = 0;
    }
    dest = huffman_encode_sym(dest, &rembits, sym);
  }
  /* 256 is special terminal symbol, pad with its prefix */
  if (rembits < 8) {
    /* if rembits < 8, we should have at least 1 buffer space
       available */
    sym = &huffman_sym_table[256];
    *dest++ |= (uint8_t)(sym->code >> (sym->nbits - rembits));
  }

  return dest;
}

void nghttp3_qpack_huffman_decode_context_init(
    nghttp3_qpack_huffman_decode_context *ctx) {
  ctx->state = 0;
  ctx->accept = 1;
}

ssize_t nghttp3_qpack_huffman_decode(nghttp3_qpack_huffman_decode_context *ctx,
                                     uint8_t *dest, const uint8_t *src,
                                     size_t srclen, int fin) {
  uint8_t *p = dest;
  size_t i;
  const nghttp3_qpack_huffman_decode_node *t;

  /* We use the decoding algorithm described in
     http://graphics.ics.uci.edu/pub/Prefix.pdf */
  for (i = 0; i < srclen; ++i) {
    t = &qpack_huffman_decode_table[ctx->state][src[i] >> 4];
    if (t->flags & NGHTTP3_QPACK_HUFFMAN_FAIL) {
      return NGHTTP3_ERR_QPACK_FATAL;
    }
    if (t->flags & NGHTTP3_QPACK_HUFFMAN_SYM) {
      *p++ = t->sym;
    }

    t = &qpack_huffman_decode_table[t->state][src[i] & 0xf];
    if (t->flags & NGHTTP3_QPACK_HUFFMAN_FAIL) {
      return NGHTTP3_ERR_QPACK_FATAL;
    }
    if (t->flags & NGHTTP3_QPACK_HUFFMAN_SYM) {
      *p++ = t->sym;
    }

    ctx->state = t->state;
    ctx->accept = (t->flags & NGHTTP3_QPACK_HUFFMAN_ACCEPTED) != 0;
  }
  if (fin && !ctx->accept) {
    return NGHTTP3_ERR_QPACK_FATAL;
  }
  return p - dest;
}
