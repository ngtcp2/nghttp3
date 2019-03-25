#!/usr/bin/env python3

for i in range(0, 256):
    print('''\
  case NGHTTP3_ERR_HTTP_MALFORMED_FRAME - {n:#04x}:
    return "ERR_HTTP_MALFORMED_FRAME_{n:#04X}";'''.format(n = i))
