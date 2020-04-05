#!/usr/bin/env python

HEADERS = [
    (':authority', 0),
    (':path', 1),
    ('age', 2),
    ('content-disposition', 3),
    ('content-length', 4),
    ('cookie', 5),
    ('date', 6),
    ('etag', 7),
    ('if-modified-since', 8),
    ('if-none-match', 9),
    ('last-modified', 10),
    ('link', 11),
    ('location', 12),
    ('referer', 13),
    ('set-cookie', 14),
    (':method', 15),
    (':method', 16),
    (':method', 17),
    (':method', 18),
    (':method', 19),
    (':method', 20),
    (':method', 21),
    (':scheme', 22),
    (':scheme', 23),
    (':status', 24),
    (':status', 25),
    (':status', 26),
    (':status', 27),
    (':status', 28),
    ('accept', 29),
    ('accept', 30),
    ('accept-encoding', 31),
    ('accept-ranges', 32),
    ('access-control-allow-headers', 33),
    ('access-control-allow-headers', 34),
    ('access-control-allow-origin', 35),
    ('cache-control', 36),
    ('cache-control', 37),
    ('cache-control', 38),
    ('cache-control', 39),
    ('cache-control', 40),
    ('cache-control', 41),
    ('content-encoding', 42),
    ('content-encoding', 43),
    ('content-type', 44),
    ('content-type', 45),
    ('content-type', 46),
    ('content-type', 47),
    ('content-type', 48),
    ('content-type', 49),
    ('content-type', 50),
    ('content-type', 51),
    ('content-type', 52),
    ('content-type', 53),
    ('content-type', 54),
    ('range', 55),
    ('strict-transport-security', 56),
    ('strict-transport-security', 57),
    ('strict-transport-security', 58),
    ('vary', 59),
    ('vary', 60),
    ('x-content-type-options', 61),
    ('x-xss-protection', 62),
    (':status', 63),
    (':status', 64),
    (':status', 65),
    (':status', 66),
    (':status', 67),
    (':status', 68),
    (':status', 69),
    (':status', 70),
    (':status', 71),
    ('accept-language', 72),
    ('access-control-allow-credentials', 73),
    ('access-control-allow-credentials', 74),
    ('access-control-allow-headers', 75),
    ('access-control-allow-methods', 76),
    ('access-control-allow-methods', 77),
    ('access-control-allow-methods', 78),
    ('access-control-expose-headers', 79),
    ('access-control-request-headers', 80),
    ('access-control-request-method', 81),
    ('access-control-request-method', 82),
    ('alt-svc', 83),
    ('authorization', 84),
    ('content-security-policy', 85),
    ('early-data', 86),
    ('expect-ct', 87),
    ('forwarded', 88),
    ('if-range', 89),
    ('origin', 90),
    ('purpose', 91),
    ('server', 92),
    ('timing-allow-origin', 93),
    ('upgrade-insecure-requests', 94),
    ('user-agent', 95),
    ('x-forwarded-for', 96),
    ('x-frame-options', 97),
    ('x-frame-options', 98),
    # Additional header fields for HTTP messaging validation
    ('host', None),
    ('connection', None),
    ('keep-alive', None),
    ('proxy-connection', None),
    ('transfer-encoding', None),
    ('upgrade', None),
    ('te', None),
    (':protocol', None),
    ('priority', None),
]

def to_enum_hd(k):
    res = 'NGHTTP3_QPACK_TOKEN_'
    for c in k.upper():
        if c == ':' or c == '-':
            res += '_'
            continue
        res += c
    return res

def build_header(headers):
    res = {}
    for k, _ in headers:
        size = len(k)
        if size not in res:
            res[size] = {}
        ent = res[size]
        c = k[-1]
        if c not in ent:
            ent[c] = []
        if k not in ent[c]:
            ent[c].append(k)

    return res

def gen_enum():
    name = ''
    print 'typedef enum {'
    for k, token in HEADERS:
        if token is None:
            print '  {},'.format(to_enum_hd(k))
        else:
            if name != k:
                name = k
                print '  {} = {},'.format(to_enum_hd(k), token)
    print '} nghttp3_qpack_token;'

def gen_index_header():
    print '''\
static int32_t lookup_token(const uint8_t *name, size_t namelen) {
  switch (namelen) {'''
    b = build_header(HEADERS)
    for size in sorted(b.keys()):
        ents = b[size]
        print '''\
  case {}:'''.format(size)
        print '''\
    switch (name[{}]) {{'''.format(size - 1)
        for c in sorted(ents.keys()):
            headers = sorted(ents[c])
            print '''\
    case '{}':'''.format(c)
            for k in headers:
                print '''\
      if (memeq("{}", name, {})) {{
        return {};
      }}'''.format(k[:-1], size - 1, to_enum_hd(k))
            print '''\
      break;'''
        print '''\
    }
    break;'''
    print '''\
  }
  return -1;
}'''

if __name__ == '__main__':
    print '''/* Don't use nghttp3_qpack_token below.  Use mkstatichdtbl.py instead */'''
    gen_enum()
    print ''
    gen_index_header()
