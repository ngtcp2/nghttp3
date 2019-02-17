#!/bin/bash
set -e

destdir=qifs/encoded/qpack-05/nghttp3
mkdir -p "$destdir"

for f in qifs/qifs/*.qif; do
    prefix=`basename ${f%.qif}`

    for maxtablesize in 0 256 512 4096; do
	for maxblocked in 0 100; do
	    echo $f $maxtablesize $maxblocked 0
	    outprefix=$prefix.out.$maxtablesize.$maxblocked
	    examples/qpack encode "$f" "$destdir/$outprefix.0" -s$maxtablesize -m$maxblocked
	    echo $f $maxtablesize $maxblocked 1
	    examples/qpack encode "$f" "$destdir/$outprefix.1" -s$maxtablesize -m$maxblocked -a
	done
    done
done
