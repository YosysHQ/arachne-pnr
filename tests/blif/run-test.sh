#!/bin/bash

set -e

arachne_pnr=../../bin/arachne-pnr

for blif in *.blif; do
  $arachne_pnr -p test.pcf $blif -o /dev/null || true
done
