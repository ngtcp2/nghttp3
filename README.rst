nghttp3
=======

nghttp3 is an implementation of HTTP/3 mapping over QUIC and QPACK
in C.

It does not depend on any particular QUIC transport implementation.

Branching strategy
------------------

As of the beginning of draft-23 development, the new branching
strategy has been introduced.  The main branch tracks the latest QUIC
draft development.  When new draft-*NN* is published, the new branch
named draft-*NN-1* is created based on the main branch.  Those
draft-*NN* branches are considered as "archived", which means that no
update is expected.  PR should be made to the main branch only.

For older draft implementations:

- `draft-32 <https://github.com/ngtcp2/nghttp3/tree/draft-32>`_
- `draft-31 <https://github.com/ngtcp2/nghttp3/tree/draft-31>`_
- `draft-30 <https://github.com/ngtcp2/nghttp3/tree/draft-30>`_
- `draft-29 <https://github.com/ngtcp2/nghttp3/tree/draft-29>`_
- `draft-28 <https://github.com/ngtcp2/nghttp3/tree/draft-28>`_
- `draft-27 <https://github.com/ngtcp2/nghttp3/tree/draft-27>`_
- `draft-25 <https://github.com/ngtcp2/nghttp3/tree/draft-25>`_
- `draft-24 <https://github.com/ngtcp2/nghttp3/tree/draft-24>`_
- `draft-23 <https://github.com/ngtcp2/nghttp3/tree/draft-23>`_
- `draft-22 <https://github.com/ngtcp2/nghttp3/tree/draft-22>`_

Documentation
-------------

`Online documentation <https://nghttp2.org/nghttp3/>`_ is available.

HTTP/3
------

This library implements HTTP/3 draft-33.  It does not support server
push.

The following extensions have been implemented:

- `Extensible Prioritization Scheme for HTTP
  <https://datatracker.ietf.org/doc/html/draft-ietf-httpbis-priority>`_
- `Bootstrapping WebSockets with HTTP/3
  <https://www.ietf.org/archive/id/draft-ietf-httpbis-h3-websockets-02.html>`_

QPACK
-----

This library implements QPACK draft-20.  It supports dynamic table.

License
-------

The MIT License

Copyright (c) 2019 nghttp3 contributors
