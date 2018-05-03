#!/bin/bash

set -x

arachne_pnr=../../bin/arachne-pnr
pass='attr-with-quoted-numeric-escape'
fail='conflicting-names-outputs empty escape-test 
multiple-models names-without-model sb-dff-model'

for t in $pass; do
  if ! $arachne_pnr -p test.pcf $t.blif -o /dev/null; then
    echo "$t failed, expected pass"
    exit 1
  fi
done

for t in $fail; do
  if $arachne_pnr -p test.pcf $t.blif -o /dev/null; then
    echo "$t passed, expected fail"
    exit 1
  fi
done
