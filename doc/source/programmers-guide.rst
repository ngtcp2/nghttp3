The nghttp3 programmers' guide
==============================

This document describes a basic usage of nghttp3 library and common
pitfalls which programmers might encounter.

Assumptions
-----------

nghttp3 is a thin HTTP/3 layer over an underlying QUIC stack.  It
relies on an underlying QUIC stack for flow control and connection
management.  Although nghttp3 is QUIC stack agnostic, it expects some
particular interfaces from QUIC stack.  We will describe them below.

QPACK operations are done behind the scenes.  Application can use
:type:`nghttp3_settings` to change the behaviour of QPACK
encoder/decoder.

We define some keywords to avoid ambiguity in this document:

* HTTP payload: HTTP request/response body
* HTTP stream data: Series of HTTP header fields, HTTP payload, and
  HTTP trailer fields, serialized into HTTP/3 wire format, which is
  passed to or received from QUIC stack.

Initialization
--------------

The :type:`nghttp3_conn` is a basic building block of nghttp3 library.
It is created per HTTP/3 connection.  If an endpoint is a client, use
`nghttp3_conn_client_new` to initialize it as client.  If it is a
server, use `nghttp3_conn_server_new` to initialize it as server.

Those initialization functions take :type:`nghttp3_callbacks`.  All
callbacks are optional, but setting no callback functions makes
nghttp3 library useless for the most cases.  We list callbacks which
effectively required to do HTTP/3 transaction below:

* :member:`acked_stream_data <nghttp3_callbacks.acked_stream_data>`:
  Application has to retain HTTP payload (HTTP request/response body)
  until they are no longer used by :type:`nghttp3_conn`.  This
  callback functions tells the largest offset of HTTP payload
  acknowledged by a remote endpoint, and no longer used.
* :member:`stream_close <nghttp3_callbacks.stream_close>`: It is
  called when a stream is closed.  It is useful to free resources
  allocated for a stream.
* :member:`recv_data <nghttp3_callbacks.recv_data>`: It is called when
  HTTP payload (HTTP request/response body) is received.
* :member:`deferred_consume <nghttp3_callbacks.deferred_consume>`: It
  is called when :type:`nghttp3_conn` consumed HTTP stream data which
  had been blocked for synchronization between streams.  Application
  has to tell QUIC stack the number of bytes consumed which affects
  flow control.  We will discuss more about this callback later when
  explaining `nghttp3_conn_read_stream`.
* :member:`recv_header <nghttp3_callbacks.recv_header>`: It is called
  when an HTTP header field is received.
* :member:`send_stop_sending <nghttp3_callbacks.send_stop_sending>`:
  It is called when QUIC STOP_SENDING frame must be sent for a
  particular stream.  Sending STOP_SENDING frame means that
  :type:`nghttp3_conn` no longer reads an incoming data for a
  particular stream.  Application has to tell QUIC stack to send
  STOP_SENDING frame.
* :member:`reset_stream <nghttp3_callbacks.reset_stream>`: It is
  called when QUIC RESET_STREAM frame must be sent for a particular
  stream.  Sending RESET_STREAM frame means that :type:`nghttp3_conn`
  stops sending any HTTP stream data to a particular stream.
  Application has to tell QUIC stack to send RESET_STREAM frame.

The initialization functions also takes :type:`nghttp3_settings` which
is a set of options to tweak HTTP3/ connection settings.
`nghttp3_settings_default` fills the default values.

The *user_data* parameter to the initialization function is an opaque
pointer and it is passed to callback functions.

Binding control streams
-----------------------

HTTP/3 requires at least 3 local unidirectional streams for a control
stream and QPACK encoder/decoder streams.

Use the following functions to bind those streams to their purposes:

* `nghttp3_conn_bind_control_stream`: Bind a given stream ID to a HTTP
  control stream.
* `nghttp3_conn_bind_qpack_streams`: Bind given 2 stream IDs to QPACK
  encoder and decoder streams.

Reading HTTP stream data
------------------------

`nghttp3_conn_read_stream` reads HTTP stream data from a particular
stream.  It returns the number of bytes "consumed".  "Consumed" means
that the those bytes are completely processed and QUIC stack can
increase the flow control credit of both stream and connection by that
amount.

The HTTP payload notified by :member:`nghttp3_callbacks.recv_data` is
not included in the return value.  This is because the consumption of
those data is done by application and nghttp3 library does not know
when that happens.

Some HTTP stream data might be consumed later because of
synchronization between streams.  In this case, those bytes are
notified by :member:`nghttp3_callbacks.deferred_consume`.

In every case, the number of consumed HTTP stream data must be
notified to QUIC stack so that it can extend flow control limits.

Writing HTTP stream data
------------------------

`nghttp3_conn_writev_stream` writes HTTP stream data to a particular
stream.  The order of streams to produce HTTP stream data is
determined by the nghttp3 library.  In general, the control streams
have higher priority.  The regular HTTP streams are ordered by
header-based HTTP priority (see
https://tools.ietf.org/html/draft-ietf-httpbis-priority-03).

When HTTP stream data is generated, its stream ID is assigned to
*\*pstream_id*.  The pointer to HTTP stream data is assigned to *vec*,
and the function returns the number of *vec* it filled.  If the
generated data is the final part of the stream, *\*pfin* gets nonzero
value.  If no HTTP stream data is generated, the function returns 0
and *\*pstream_id* gets -1.

The function might return 0 and *\*pstream_id* has proper stream ID
and *\*pfin* set to nonzero.  In this case, no data is written, but it
signals the end of the stream.  Even though no data is written, QUIC
stack should be notified of the end of the stream.

The produced HTTP stream data is passed to QUIC stack.  Then call
`nghttp3_conn_add_write_offset` with the number of bytes accepted by
QUIC stack.  This must be done even when the written data is 0 bytes
with fin (refer to the previous paragraph for this corner case).

If QUIC stack indicates that a stream is blocked by stream level flow
control limit, call `nghttp3_conn_block_stream`.  It makes the library
not to generate HTTP stream data for the stream.  Call
`nghttp3_conn_unblock_stream` when stream level flow control limit is
increased.

If QUIC stack indicates that the write side of stream is closed, call
`nghttp3_conn_shutdown_stream_write` instead of
`nghttp3_conn_block_stream` so that the stream never be scheduled in
the future.

Creating HTTP request or response
---------------------------------

In order to create HTTP request, client application calls
`nghttp3_conn_submit_request`.  :type:`nghttp3_data_reader` is used to
send HTTP payload (HTTP request body).

Similarly, server application calls `nghttp3_conn_submit_response` to
create HTTP response.  :type:`nghttp3_data_reader` is also used to
send HTTP payload (HTTP response body).

In both cases, if :type:`nghttp3_data_reader` is not provided, no HTTP
payload is generated.

The :member:`nghttp3_data_reader.read_data` is a callback function to
generate HTTP payload.  Application must retain the data passed to the
library until those data are acknowledged by
:member:`nghttp3_callbacks.acked_stream_data`.  When no data is
available but will become available in the future, application returns
:macro:`NGHTTP3_ERR_WOULDBLOCK` from this callback.  Then the callback
is not called for the particular stream until
`nghttp3_conn_resume_stream` is called.

Reading HTTP request or response
--------------------------------

The :member:`nghttp3_callbacks.recv_header` is called when an HTTP
header field is received.

The :member:`nghttp3_callbacks.recv_data` is called when HTTP payload
is received.

Acknowledgement of HTTP stream data
-----------------------------------

QUIC stack must provide an interface to notify the amount of data
acknowledged by a remote endpoint.  `nghttp3_conn_add_ack_offset` must
be called with the largest offset of acknowledged HTTP stream data.

Handling QUIC stream events
---------------------------

If underlying QUIC stream is closed, call `nghttp3_conn_close_stream`.

If underlying QUIC stream is reset by a remote endpoint (that is when
RESET_STREAM is received) or no longer read by a local endpoint (that
is when STOP_SENDING is sent), call
`nghttp3_conn_shutdown_stream_read`.

Closing HTTP/3 connection gracefully
------------------------------------

`nghttp3_conn_submit_shutdown_notice` creates a message to a remote
endpoint that HTTP/3 connection is going down.  The receiving endpoint
should stop sending HTTP request after reading this signal.  After a
couple of RTTs, call `nghttp3_conn_submit_shutdown` to start graceful
shutdown.  After calling this function, the local endpoint starts
rejecting new incoming streams.  The existing streams are processed
normally.  When all those streams are completely processed, the
connection can be closed.  Clients inherently know whether their
requests have completed or not.  For server, `nghttp3_conn_is_drained`
tells whether all those streams have been completely processed.  When
it returns nonzero, the connection can be closed.
