#!/usr/bin/env python3
import sys

def name(i):
    if i < 0x21:
        return \
            ['NUL ', 'SOH ', 'STX ', 'ETX ', 'EOT ', 'ENQ ', 'ACK ', 'BEL ',
             'BS  ', 'HT  ', 'LF  ', 'VT  ', 'FF  ', 'CR  ', 'SO  ', 'SI  ',
             'DLE ', 'DC1 ', 'DC2 ', 'DC3 ', 'DC4 ', 'NAK ', 'SYN ', 'ETB ',
             'CAN ', 'EM  ', 'SUB ', 'ESC ', 'FS  ', 'GS  ', 'RS  ', 'US  ',
             'SPC '][i]
    elif i == 0x7f:
        return 'DEL '

for i in range(256):
    if (0x41 <= i and i <= 0x5a) or (0x61 <= i and i <= 0x7a) or \
       (0x30 <= i and i <= 0x39) or \
       chr(i) in ['-', '.', '_', '~',
                  '!', '$', '&', "'", '(', ')',
                  '*', '+', ',', ';', '=',
                  '/', '?',
                  '%',
                  ':', '@']:
        sys.stdout.write('1 /* {}    */, '.format(chr(i)))
    elif 0x80 <= i:
        sys.stdout.write('0 /* {} */, '.format(hex(i)))
    elif i == 0x7f or i <= 0x20:
        sys.stdout.write('0 /* {} */, '.format(name(i)))
    else:
        sys.stdout.write('0 /* {}    */, '.format(chr(i)))
    if (i + 1)%4 == 0:
        sys.stdout.write('\n')
