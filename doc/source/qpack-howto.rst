QPACK How-To
============

Using QPACK encoder
-------------------

Firstly, create QPACK encoder by calling `nghttp3_qpack_encoder_new`.
It requires *hard_max_dtable_size* parameter.  When in doubt, pass
4096 for this tutorial.  Optionally, call
`nghttp3_qpack_encoder_set_max_dtable_capacity` to set the maximum
size of dynamic table.  You can also call
`nghttp3_qpack_encoder_set_max_blocked_streams` to set the maximum
number of streams that can be blocked.

In order to encode HTTP header fields, they must be stored in an array
of :type:`nghttp3_nv`.  Then call `nghttp3_qpack_encoder_encode`.  It
writes 3 buffers; *pbuf*, *rbuf*, and *ebuf*.  They are a header block
prefix, request stream, and encoder stream respectively.  A header
block prefix and request stream must be sent in this order to a stream
denoted by *stream_id* passed to the function.  Encoder stream must be
sent to the encoder stream you setup.

In order to read decoder stream, call
`nghttp3_qpack_encoder_read_decoder`.

Once QPACK encoder is no longer used, call `nghttp3_qpack_encoder_del`
to free up memory allocated for it.

Using QPACK decoder
-------------------

`nghttp3_qpack_decoder_new` will create new QPACK decoder.  It
requires *hard_max_dtable_size* and *max_blocked* parameters.  When in
doubt, pass 4096 and 0 respectively for this tutorial.

In order to read encoder stream, call
`nghttp3_qpack_decoder_read_encoder`.  This might update dynamic
table, but does not emit any header fields.

In order to read request stream, call
`nghttp3_qpack_decoder_read_request`.  This is the function to emit
header fields.  *sctx* stores a per-stream decoder state and must be
created by `nghttp3_qpack_stream_context_new`.  It identifies a single
encoded header block in a particular request stream.  *fin* must be
nonzero if and only if a passed data contains the last part of encoded
header block.

The scope of :type:`nghttp3_qpack_stream_context` is per header block,
but `nghttp3_qpack_stream_context_reset` resets its state and can be
reused for an another header block in the same stream.  In general,
you can reset it when you see that
:macro:`NGHTTP3_QPACK_DECODE_FLAG_FINAL` is set in *\*pflags*.  When
:type:`nghttp3_qpack_stream_context` is no longer necessary, call
`nghttp3_qpack_stream_context_del` to free up its resource.

`nghttp3_qpack_decoder_read_request` succeeds, *\*pflags* is assigned.
If it has :macro:`NGHTTP3_QPACK_DECODE_FLAG_EMIT` set, a header field
is emitted and stored in the buffer pointed by *nv*.  If *\*pflags*
has :macro:`NGHTTP3_QPACK_DECODE_FLAG_FINAL` set, all header fields
have been successfully decoded.  If *\*pflags* has
:macro:`NGHTTP3_QPACK_DECODE_FLAG_BLOCKED` set, decoding is blocked
due to required insert count, which means that more data must be read
by `nghttp3_qpack_decoder_read_encoder`.

`nghttp3_qpack_decoder_read_request` returns the number of bytes read.
When a header field is emitted, it might read data partially.
Applciation has to call the function repeatedly by adjusting the
pointer to data and its length until the function consumes all data or
:macro:`NGHTTP3_QPACK_DECODE_FLAG_BLOCKED` is set in *\*pflags*.

If *nv* is assigned, its :member:`nv->name <nghttp3_qpack_nv.name>`
and :member:`nv->value <nghttp3_qpack_nv.value>` are reference counted
and already incremented by 1.  If application finishes processing
these values, it must call `nghttp3_rcbuf_decref(nv->name)
<nghttp3_rcbuf_decref>` and `nghttp3_rcbuf_decref(nv->value)
<nghttp3_rcbuf_decref>`.

If an application has no interest to decode header fields for a
particular stream, call `nghttp3_qpack_decoder_cancel_stream`.

In order to tell decoding state to an encoder, QPACK decoder has to
write decoder stream by calling `nghttp3_qpack_decoder_write_decoder`.

Once QPACK decoder is no longer used, call `nghttp3_qpack_decoder_del`
to free up memory allocated for it.
