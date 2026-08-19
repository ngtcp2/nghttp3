#!/bin/bash -eu

FUZZERS=(
    fuzz_http3serverreq
    fuzz_qpackdecoder
)

for fuzzer in "${FUZZERS[@]}"; do
    $CXX $CXXFLAGS -std=c++23 -Ilib/includes -Ilib -I. -DHAVE_CONFIG_H \
         fuzz/${fuzzer}.cc -o $OUT/${fuzzer} \
         $LIB_FUZZING_ENGINE lib/.libs/libnghttp3.a
done
