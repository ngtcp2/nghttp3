# ngtcp3
#
# Copyright (c) 2019 nghttp3 contributors
# Copyright (c) 2017 ngtcp2 contributors
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
EXTRA_DIST = CMakeLists.txt

if ENABLE_EXAMPLES

AM_CFLAGS = $(WARNCFLAGS) $(DEBUGCFLAGS)
AM_CXXFLAGS = $(WARNCXXFLAGS) $(DEBUGCFLAGS)
AM_CPPFLAGS = \
	-I$(top_srcdir)/lib/includes \
	-I$(top_builddir)/lib/includes \
	@DEFS@
AM_LDFLAGS = -no-install
LDADD = $(top_builddir)/lib/libnghttp3.la

noinst_PROGRAMS = qpack

qpack_SOURCES = \
	qpack.cc qpack.h \
	qpack_encode.cc qpack_encode.h \
	qpack_decode.cc qpack_decode.h \
	template.h \
	util.cc util.h

endif # ENABLE_EXAMPLES
