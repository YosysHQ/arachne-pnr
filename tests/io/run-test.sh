#!/bin/bash

set -ex

arachne_pnr=../../bin/arachne-pnr
tests='tri inout_in inout_out inout_z inout_inout inout_chain inout_tbuf'

for t in $tests; do
  $arachne_pnr $t.blif -o $t.txt
  icebox_vlog -s $t.txt >out-$t.v
  if [ -e $t.v ]; then
    diff -u $t.v out-$t.v
  else
    cp out-$t.v $t.v
  fi
done
