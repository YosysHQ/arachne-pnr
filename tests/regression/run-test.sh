#!/bin/bash

set -ex

: ${CURRENT_SOURCE_DIR:=.}
: ${ARACHNE_PNR:=../../build/bin/arachne-pnr}
: ${ICEPACK:=icepack}

for i in $CURRENT_SOURCE_DIR/*.blif; do
    base=${i%.*}
    $ARACHNE_PNR $i -o ${base}.txt
    $ICEPACK ${base}.txt ${base}.bin
done
