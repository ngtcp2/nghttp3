nghttp3
=======

nghttp3 is an implementation of HTTP/3 mapping over QUIC and QPACK
in C.

It does not depend on any particular QUIC transport implementation.

HTTP/3
------

This library implements HTTP/3 draft-20.  It can exchange basic HTTP
request, response and server push, but does not fully implement the
specification at the moment.

QPACK
-----

This library implements QPACK draft-08.  It supports dynamic table.

License
-------

The MIT License

Copyright (c) 2019 nghttp3 contributors
