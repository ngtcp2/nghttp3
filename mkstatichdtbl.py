#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# This scripts reads static table entries [1] and generates
# nghttp3_hd_static_entry table.  This table is used in
# lib/nghttp3_hd.c.
#
# [1] https://quicwg.org/base-drafts/draft-ietf-quic-qpack.html#rfc.appendix.A

import re, sys

def hd_map_hash(name):
  h = 2166136261

  # FNV hash variant: http://isthe.com/chongo/tech/comp/fnv/
  for c in name:
    h ^= ord(c)
    h *= 16777619
    h &= 0xffffffff

  return h

class Header:
  def __init__(self, idx, name, value):
    self.idx = idx
    self.name = name
    self.value = value
    self.token = -1

entries = []
for line in sys.stdin:
    m = re.match(r'(\d+)\s+(\S+)\s+(\S.*)?', line)
    val = m.group(3).strip() if m.group(3) else ''
    entries.append(Header(int(m.group(1)), m.group(2), val))

token_entries = sorted(entries, key=lambda ent: ent.name)

token = 0
seq = 0
name = token_entries[0].name
for i, ent in enumerate(token_entries):
    if name != ent.name:
        name = ent.name
        token = seq
    seq += 1
    ent.token = token

def to_enum_hd(k):
    res = 'NGHTTP3_QPACK_TOKEN_'
    for c in k.upper():
        if c == ':' or c == '-':
            res += '_'
            continue
        res += c
    return res

def gen_enum(entries):
    used = {}
    print('typedef enum {')
    for ent in entries:
        if ent.token is None:
            print('  {},'.format(to_enum_hd(ent.name)))
        else:
            if ent.name in used:
                continue
            used[ent.name] = True
            print('  {} = {},'.format(to_enum_hd(ent.name), ent.token))
    print('} nghttp3_qpack_token;')

gen_enum(entries)

print()

print('static nghttp3_qpack_static_entry token_stable[] = {')
for i, ent in enumerate(token_entries):
    print('MAKE_STATIC_ENT("{}", "{}", {}, {}, {}u),'\
          .format(ent.name, ent.value, ent.idx, to_enum_hd(ent.name), hd_map_hash(ent.name)))
print('};')

print()

print('static nghttp3_qpack_static_header stable[] = {')
for ent in entries:
    print('MAKE_STATIC_HD("{}", "{}", {}),'\
          .format(ent.name, ent.value, to_enum_hd(ent.name)))
print('};')
