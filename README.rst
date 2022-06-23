nghttp3
=======

nghttp3 is an implementation of `RFC 9114
<https://datatracker.ietf.org/doc/html/rfc9114>`_ HTTP/3 mapping over
QUIC and `RFC 9204 <https://datatracker.ietf.org/doc/html/rfc9204>`_
QPACK in C.

It does not depend on any particular QUIC transport implementation.

Documentation
-------------

`Online documentation <https://nghttp2.org/nghttp3/>`_ is available.

HTTP/3
------

This library implements `RFC 9114
<https://datatracker.ietf.org/doc/html/rfc9114>`_ HTTP/3.  It does not
support server push.

The following extensions have been implemented:

- `Extensible Prioritization Scheme for HTTP
  <https://datatracker.ietf.org/doc/html/rfc9218>`_
- `Bootstrapping WebSockets with HTTP/3
  <https://datatracker.ietf.org/doc/html/rfc9220>`_

QPACK
-----

This library implements `RFC 9204
<https://datatracker.ietf.org/doc/html/rfc9204>`_ QPACK.  It supports
dynamic table.

License
-------

The MIT License

Copyright (c) 2019 nghttp3 contributors
