# Threat model

## What this project does and where untrusted input enters

This is RFC 9114 HTTP/3 implementation.  It also implements RFC 9204
QPACK.

It reads HTTP/3 stream from QUIC stack which is totally untrusted.

## Components that matter most / least

The primary object is nghttp3_conn that is created per HTTP/3 session
which is on top of QUIC connection.  Both read and write matter, but
read should be more important because it reads untrusted inputs.
nghttp3 itself does not do any I/O (aka sans-I/O).  Instead, it reads
HTTP/3 stream provided by QUIC stack and writes pure byte string
HTTP/3 stream that is passed to QUIC stack.

- Reading HTTP/3 stream: HTTP/3 operates on top of QUIC.  QUIC stack
  passes QUIC stream data to nghttp3 via nghttp3_conn_read_stream2.

- Writing HTTP/3 stream: The library writes HTTP/3 stream which
  consists of HTTP/3 frames.  They are passed to QUIC stack.

- Header compression/decompression: HEADERS frame carries the
  compressed header block.  It is called a request stream.  QPACK also
  uses 2 unidirectional streams for encoder and decoder streams.
  QPACK encoder writes into encoder stream to manipulate dynamic
  table.  QPACK decoder reads the encoder stream to mirror the dynamic
  table changes that the encoder made.  QPACK decoder writes
  acknowledgements into decoder stream.  When HEADERS arrives on
  request stream, QPACK decoder decompresses it.

- RFC 9218 priorities: nghttp3 supports stream prioritization.
  Prioritization is done via control stream.  The stream may be
  created before nghttp3 reads its request stream.

- nghttp3 somewhat relies on the QUIC stack to operate correctly.

## How to exercise it

tests contains unit tests.  lib/includes/ngtcp2/nghttp3.h contains
public API with documentation.

doc/source/programmers-guide.rst describes the basic usage of this
library.  doc/source/qpack-howto.rst describes the basic usage of
QPACK functions.

The best examples of this library exist under examples directory of
ngtcp2 project (see https://github.com/ngtcp2/ngtcp2).

## How you rate severity

Any buffer overflow, use-after-free, or double free is high+ at a
minimum. If the overflow is controlled and may lead to RCE then it
should be critical. All DoS is high.

## Anything to leave alone

Do not report anything about test code that is under tests directory.
