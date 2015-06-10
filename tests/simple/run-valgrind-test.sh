#!/bin/bash

set -ex

: ${ARACHNE_PNR:=../../build/arachne-pnr}
: ${ICEPACK:=icepack}
: ${VALGRIND:=valgrind}

$VALGRIND $ARACHNE_PNR sb_up3down5.blif -o sb_up3down5_valgrind.txt
$ICEPACK sb_up3down5_valgrind.txt sb_up3down5_valgrind.bin
