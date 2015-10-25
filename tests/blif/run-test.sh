#!/bin/bash

set -x

arachne_pnr=../../bin/arachne-pnr

for blif in *.blif; do
  $arachne_pnr -p test.pcf $blif -o /dev/null
  if [ x"$?" != x"1" ]; then
      echo "error, stopping."
      exit 1
  fi
done
