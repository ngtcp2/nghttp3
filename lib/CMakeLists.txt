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

add_subdirectory(includes)

include_directories(
  "${CMAKE_CURRENT_SOURCE_DIR}/includes"
  "${CMAKE_CURRENT_BINARY_DIR}/includes"
)

add_definitions(-DBUILDING_NGHTTP3)

set(nghttp3_SOURCES
  nghttp3_rcbuf.c
  nghttp3_mem.c
  nghttp3_str.c
  nghttp3_conv.c
  nghttp3_buf.c
  nghttp3_ringbuf.c
  nghttp3_pq.c
  nghttp3_map.c
  nghttp3_ksl.c
  nghttp3_qpack.c
  nghttp3_qpack_huffman.c
  nghttp3_qpack_huffman_data.c
  nghttp3_err.c
  nghttp3_debug.c
  nghttp3_conn.c
  nghttp3_stream.c
  nghttp3_frame.c
  nghttp3_tnode.c
  nghttp3_vec.c
  nghttp3_gaptr.c
  nghttp3_idtr.c
  nghttp3_range.c
  nghttp3_http.c
  nghttp3_version.c
  nghttp3_balloc.c
  nghttp3_opl.c
  nghttp3_objalloc.c
  nghttp3_unreachable.c
  sfparse/sfparse.c
)

# Public shared library
if(ENABLE_SHARED_LIB)
  add_library(nghttp3 SHARED ${nghttp3_SOURCES})
  set_target_properties(nghttp3 PROPERTIES
    COMPILE_FLAGS "${WARNCFLAGS}"
    VERSION ${LT_VERSION} SOVERSION ${LT_SOVERSION}
    C_VISIBILITY_PRESET hidden
  )

  install(TARGETS nghttp3
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()

# Static library (for unittests because of symbol visibility)
add_library(nghttp3_static STATIC ${nghttp3_SOURCES})
set_target_properties(nghttp3_static PROPERTIES
  COMPILE_FLAGS "${WARNCFLAGS}"
  VERSION ${LT_VERSION} SOVERSION ${LT_SOVERSION}
  ARCHIVE_OUTPUT_NAME nghttp3${STATIC_LIB_SUFFIX}
  )
target_compile_definitions(nghttp3_static PUBLIC "-DNGHTTP3_STATICLIB")
if(ENABLE_STATIC_LIB)
  install(TARGETS nghttp3_static
    DESTINATION "${CMAKE_INSTALL_LIBDIR}")
endif()

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/libnghttp3.pc"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
