# nghttp3
#
# Copyright (c) 2019 nghttp3
# Copyright (c) 2016 ngtcp2
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
SUBDIRS = includes

EXTRA_DIST = CMakeLists.txt sfparse/COPYING config.cmake.in

AM_CFLAGS = $(WARNCFLAGS) $(DEBUGCFLAGS) $(EXTRACFLAG)
AM_CPPFLAGS = -I$(srcdir)/includes -I$(builddir)/includes -DBUILDING_NGHTTP3

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libnghttp3.pc
DISTCLEANFILES = $(pkgconfig_DATA)

lib_LTLIBRARIES = libnghttp3.la

OBJECTS = \
	nghttp3_rcbuf.c \
	nghttp3_mem.c \
	nghttp3_str.c \
	nghttp3_conv.c \
	nghttp3_buf.c \
	nghttp3_ringbuf.c \
	nghttp3_pq.c \
	nghttp3_map.c \
	nghttp3_ksl.c \
	nghttp3_qpack.c \
	nghttp3_qpack_huffman.c \
	nghttp3_qpack_huffman_data.c \
	nghttp3_err.c \
	nghttp3_debug.c \
	nghttp3_conn.c \
	nghttp3_stream.c \
	nghttp3_frame.c \
	nghttp3_tnode.c \
	nghttp3_vec.c \
	nghttp3_gaptr.c \
	nghttp3_idtr.c \
	nghttp3_range.c \
	nghttp3_http.c \
	nghttp3_version.c \
	nghttp3_balloc.c \
	nghttp3_opl.c \
	nghttp3_objalloc.c \
	nghttp3_unreachable.c \
	sfparse/sfparse.c
HFILES = \
	nghttp3_rcbuf.h \
	nghttp3_mem.h \
	nghttp3_str.h \
	nghttp3_conv.h \
	nghttp3_buf.h \
	nghttp3_ringbuf.h \
	nghttp3_pq.h \
	nghttp3_map.h \
	nghttp3_ksl.h \
	nghttp3_qpack.h \
	nghttp3_qpack_huffman.h \
	nghttp3_err.h \
	nghttp3_debug.h \
	nghttp3_conn.h \
	nghttp3_stream.h \
	nghttp3_frame.h \
	nghttp3_tnode.h \
	nghttp3_vec.h \
	nghttp3_gaptr.h \
	nghttp3_idtr.h \
	nghttp3_range.h \
	nghttp3_http.h \
	nghttp3_balloc.h \
	nghttp3_opl.h \
	nghttp3_objalloc.h \
	nghttp3_unreachable.h \
	sfparse/sfparse.h \
	nghttp3_macro.h

libnghttp3_la_SOURCES = $(HFILES) $(OBJECTS)
libnghttp3_la_LDFLAGS = -no-undefined \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)
