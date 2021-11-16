#!/bin/bash -eu

autoreconf -i
./configure
make -j$(nproc)

$CXX $CXXFLAGS -std=c++11 -Ilib/includes \
     fuzz/fuzz_http3serverreq.cc -o $OUT/fuzz_http3serverreq \
     $LIB_FUZZING_ENGINE lib/.libs/libnghttp3.a

zip -j $OUT/fuzz_http3serverreq_seed_corpus.zip fuzz/corpus/fuzz_http3serverreq/*
