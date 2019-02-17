#!/bin/bash
set -e

for f in qifs/encoded/qpack-06/*/*; do
    echo $f

    name=`basename "$f"`
    IFS='.' read -ra params <<< "$name"
    [ "${params[1]}" = "out" ] || continue
    prefix=${params[0]}
    maxtablesize=${params[2]}
    maxblocked=${params[3]}
    immediateack=${params[4]}

    opts="-s$maxtablesize -m$maxblocked"
    if [ "$immediateack" = "1" ]; then
	opts="$opts -a"
    fi

    examples/qpack decode "$f" qpack-check.out $opts
    qifs/bin/sort-qif.pl --strip-comments qpack-check.out > qpack-check-canonical.out
    diff -u qpack-check-canonical.out "qifs/qifs/$prefix.qif"
done
