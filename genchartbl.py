#!/usr/bin/env python3
import sys
import string

def name(i):
    if i < 0x21:
        return \
            ['NUL ', 'SOH ', 'STX ', 'ETX ', 'EOT ', 'ENQ ', 'ACK ', 'BEL ',
             'BS  ', 'HT  ', 'LF  ', 'VT  ', 'FF  ', 'CR  ', 'SO  ', 'SI  ',
             'DLE ', 'DC1 ', 'DC2 ', 'DC3 ', 'DC4 ', 'NAK ', 'SYN ', 'ETB ',
             'CAN ', 'EM  ', 'SUB ', 'ESC ', 'FS  ', 'GS  ', 'RS  ', 'US  ',
             'SPC '][i]
    if i == 0x7f:
        return 'DEL '

def gentbl(tblname, pred):
    sys.stdout.write('''\
/* Generated by genchartbl.py */
static const int {}[] = {{
'''.format(tblname))

    for i in range(256):
        if pred(chr(i)):
            v = 1
        else:
            v = 0

        if 0x21 <= i and i < 0x7f:
            sys.stdout.write('{} /* {}    */, '.format(v, chr(i)))
        elif 0x80 <= i:
            sys.stdout.write('{} /* {} */, '.format(v, hex(i)))
        else:
            sys.stdout.write('{} /* {} */, '.format(v, name(i)))
        if (i + 1)%4 == 0:
            sys.stdout.write('\n')

    sys.stdout.write('};\n')

def sf_key():
    gentbl('SF_KEY_CHARS', lambda c: c in string.ascii_lowercase or
           c in string.digits or c in '_-.*')

def sf_dquote():
    gentbl('SF_DQUOTE_CHARS', lambda c: (0x20 <= ord(c) and ord(c) <= 0x21) or
           (0x23 <= ord(c) and ord(c) <= 0x5b) or
           (0x5d <= ord(c) and ord(c) <= 0x7e))

def sf_token():
    gentbl('SF_TOKEN_CHARS', lambda c: c in "!#$%&'*+-.^_`|~:/" or
           c in string.digits or c in string.ascii_letters)

def sf_byteseq():
    gentbl('SF_BYTESEQ_CHARS', lambda c: c in string.ascii_letters or
           c in string.digits or c in '+/=')

sf_key()
sys.stdout.write('\n')

sf_dquote()
sys.stdout.write('\n')

sf_token()
sys.stdout.write('\n')

sf_byteseq()
sys.stdout.write('\n')
