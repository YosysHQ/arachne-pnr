#!/bin/bash

set -ex

: ${ARACHNE_PNR:=../../arachne-pnr}
: ${ICEPACK:=icepack}

for i in *.blif; do
    base=${i%.*}
    $ARACHNE_PNR $i -o ${base}.txt
    $ICEPACK ${base}.txt ${base}.bin
done
