# nghttp3
#
# Copyright (c) 2019 nghttp3 contributors
# Copyright (c) 2016 ngtcp2 contributors
# Copyright (c) 2012 nghttp2 contributors
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
# LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
# OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
# WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
EXTRA_DIST = CMakeLists.txt munit/munit.c munit/munit.h munit/COPYING

check_PROGRAMS = main

OBJECTS = \
	main.c \
	nghttp3_qpack_test.c \
	nghttp3_conn_test.c \
	nghttp3_stream_test.c \
	nghttp3_tnode_test.c \
	nghttp3_http_test.c \
	nghttp3_conv_test.c \
	nghttp3_settings_test.c \
	nghttp3_callbacks_test.c \
	nghttp3_test_helper.c \
	munit/munit.c
HFILES = \
	nghttp3_qpack_test.h \
	nghttp3_conn_test.h \
	nghttp3_stream_test.h \
	nghttp3_tnode_test.h \
	nghttp3_http_test.h \
	nghttp3_conv_test.h \
	nghttp3_settings_test.h \
	nghttp3_callbacks_test.h \
	nghttp3_test_helper.h \
	munit/munit.h

main_SOURCES = $(HFILES) $(OBJECTS)

# With static lib disabled and symbol hiding enabled, we have to link object
# files directly because the tests use symbols not included in public API.
main_LDADD = ${top_builddir}/lib/.libs/*.o \
	${top_builddir}/lib/sfparse/.libs/*.o
main_LDFLAGS = -static

AM_CFLAGS = $(WARNCFLAGS) \
	-I${top_srcdir}/lib \
	-I${top_srcdir}/lib/includes \
	-I${top_srcdir}/tests/munit \
	-I${top_builddir}/lib/includes \
	-DBUILDING_NGHTTP3 \
	@DEFS@
AM_LDFLAGS = -no-install

TESTS = main
