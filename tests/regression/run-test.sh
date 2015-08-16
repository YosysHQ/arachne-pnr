#!/bin/bash

set -ex

arachne_pnr=../../bin/arachne-pnr

for d in 1k 8k; do
    rm -rf $d
    mkdir $d
    for i in *.blif; do
	base=${i%.*}
	$arachne_pnr -d $d $i -o $d/${base}.txt
	icepack $d/${base}.txt $d/${base}.bin
    done
done
