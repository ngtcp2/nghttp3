#!/bin/bash -eu

autoreconf -i
./configure --disable-dependency-tracking
make -j$(nproc)

"$(dirname "$(realpath "${BASH_SOURCE[0]}")")"/build_fuzzer.sh

zip -j $OUT/fuzz_http3serverreq_seed_corpus.zip fuzz/corpus/fuzz_http3serverreq/*
zip -j $OUT/fuzz_qpackdecoder_seed_corpus.zip fuzz/corpus/fuzz_qpackdecoder/*
