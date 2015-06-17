#!/bin/bash

set -ex

: ${CURRENT_SOURCE_DIR:=.}
: ${ARACHNE_PNR:=../../build/bin/arachne-pnr}
: ${ICEPACK:=icepack}

# sb_up3down5.blif
$ARACHNE_PNR $CURRENT_SOURCE_DIR/sb_up3down5.blif -o sb_up3down5.txt
$ICEPACK sb_up3down5.txt sb_up3down5.bin

$ARACHNE_PNR -l $CURRENT_SOURCE_DIR/sb_up3down5.blif -o sb_up3down5_l.txt
$ICEPACK sb_up3down5_l.txt sb_up3down5_l.bin

$ARACHNE_PNR $CURRENT_SOURCE_DIR/sb_up3down5.blif -B sb_up3down5_packed.blif -o sb_up3down5.txt
$ARACHNE_PNR $CURRENT_SOURCE_DIR/sb_up3down5_packed.blif -o sb_up3down5_packed.txt
$ICEPACK sb_up3down5_packed.txt sb_up3down5_packed.bin


$ARACHNE_PNR $CURRENT_SOURCE_DIR/carry.blif -o carry.txt
$ICEPACK carry.txt carry.bin

$ARACHNE_PNR $CURRENT_SOURCE_DIR/bram.blif -o bram.txt
$ICEPACK bram.txt bram.bin
