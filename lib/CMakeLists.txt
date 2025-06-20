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
  nghttp3_settings.c
  nghttp3_callbacks.c
  sfparse/sfparse.c
)

set(NGHTTP3_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(NGHTTP3_VERSION_CONFIG "${NGHTTP3_GENERATED_DIR}/${PROJECT_NAME}ConfigVersion.cmake")
set(NGHTTP3_PROJECT_CONFIG "${NGHTTP3_GENERATED_DIR}/${PROJECT_NAME}Config.cmake")
set(NGHTTP3_TARGETS_EXPORT_NAME "${PROJECT_NAME}Targets")
set(NGHTTP3_CONFIG_INSTALL_DIR "lib/cmake/${PROJECT_NAME}")
set(NGHTTP3_NAMESPACE "${PROJECT_NAME}::")
set(NGHTTP3_VERSION ${PROJECT_VERSION})

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  "${NGHTTP3_VERSION_CONFIG}" VERSION ${NGHTTP3_VERSION} COMPATIBILITY SameMajorVersion
)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/config.cmake.in" "${NGHTTP3_PROJECT_CONFIG}" @ONLY)

# Install cmake config files
install(
  FILES "${NGHTTP3_PROJECT_CONFIG}" "${NGHTTP3_VERSION_CONFIG}"
  DESTINATION "${NGHTTP3_CONFIG_INSTALL_DIR}")

# Public shared library
if(ENABLE_SHARED_LIB)
  add_library(nghttp3 SHARED ${nghttp3_SOURCES})
  set_target_properties(nghttp3 PROPERTIES
    COMPILE_FLAGS "${WARNCFLAGS}"
    VERSION ${LT_VERSION} SOVERSION ${LT_SOVERSION}
    C_VISIBILITY_PRESET hidden
  )

  target_include_directories(nghttp3 INTERFACE $<INSTALL_INTERFACE:include>)

  install(TARGETS nghttp3
    EXPORT ${NGHTTP3_TARGETS_EXPORT_NAME}
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()

if(ENABLE_STATIC_LIB)
  # Public static library
  add_library(nghttp3_static STATIC ${nghttp3_SOURCES})
  set_target_properties(nghttp3_static PROPERTIES
    COMPILE_FLAGS "${WARNCFLAGS}"
    VERSION ${LT_VERSION} SOVERSION ${LT_SOVERSION}
    ARCHIVE_OUTPUT_NAME nghttp3${STATIC_LIB_SUFFIX}
    )

  target_include_directories(nghttp3_static INTERFACE $<INSTALL_INTERFACE:include>)

  target_compile_definitions(nghttp3_static PUBLIC "-DNGHTTP3_STATICLIB")

  install(TARGETS nghttp3_static
    EXPORT ${NGHTTP3_TARGETS_EXPORT_NAME}
    DESTINATION "${CMAKE_INSTALL_LIBDIR}")
endif()

install(
  EXPORT "${NGHTTP3_TARGETS_EXPORT_NAME}"
  NAMESPACE "${NGHTTP3_NAMESPACE}"
  DESTINATION "${NGHTTP3_CONFIG_INSTALL_DIR}")

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/libnghttp3.pc"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
