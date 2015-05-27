#!/bin/bash

set -ex

arachne_pnr=../../bin/arachne-pnr

for i in *.blif; do
    base=${i%.*}
    $arachne_pnr $i -o ${base}.txt
    icepack ${base}.txt ${base}.bin
done
