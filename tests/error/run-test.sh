#!/bin/bash

set -x

arachne_pnr=../../bin/arachne-pnr

$arachne_pnr -p dup_pin.pcf dup_pin.blif -o /dev/null
if [ x"$?" != x"1" ]; then
    echo "error, stopping."
    exit 1
fi

$arachne_pnr carry_loop.blif -o /dev/null
if [ x"$?" != x"1" ]; then
    echo "error, stopping."
    exit 1
fi

$arachne_pnr tri.blif -o /dev/null
if [ x"$?" != x"1" ]; then
    echo "error, stopping."
    exit 1
fi
