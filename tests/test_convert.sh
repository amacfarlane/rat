#!/bin/sh

for c in 0 1 2; do
    for ichannels in 1 2; do
	for irate in 8000 16000 32000 48000; do
	    for ochannels in 1 2; do
		for orate in 8000 16000 32000 48000; do
		    echo "#Testing $c: $ichannels,$irate -> $ochannels,$orate" > /dev/stderr
		    ./test_convert -c $c \
			-ifmt channels=$ichannels,rate=$irate \
			-ofmt channels=$ochannels,rate=$orate
		    if [ $? != 0 ] ; then
			exit
		    fi
		done
	    done
	done
    done
done