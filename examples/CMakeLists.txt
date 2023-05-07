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

if(ENABLE_EXAMPLES)
  include_directories(
    ${CMAKE_SOURCE_DIR}/lib/includes
    ${CMAKE_BINARY_DIR}/lib/includes
  )

  if(ENABLE_SHARED_LIB)
    link_libraries(
      nghttp3
    )
  elseif(HAVE_CUNIT OR ENABLE_STATIC_LIB)
    link_libraries(
      nghttp3_static
    )
  endif()

  set(qpack_SOURCES
    qpack.cc
    qpack_encode.cc
    qpack_decode.cc
    util.cc
  )

  add_executable(qpack ${qpack_SOURCES})
  set_target_properties(qpack PROPERTIES
    COMPILE_FLAGS "${WARNCXXFLAGS}"
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
  )

  # TODO prevent qpack example from being installed?
endif()
