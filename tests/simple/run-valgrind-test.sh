#!/bin/bash

set -ex

: ${CURRENT_SOURCE_DIR:=.}
: ${ARACHNE_PNR:=../../build/bin/arachne-pnr}
: ${ICEPACK:=icepack}
: ${VALGRIND:=valgrind}

$VALGRIND $ARACHNE_PNR $CURRENT_SOURCE_DIR/sb_up3down5.blif -o sb_up3down5_valgrind.txt
$ICEPACK sb_up3down5_valgrind.txt sb_up3down5_valgrind.bin
