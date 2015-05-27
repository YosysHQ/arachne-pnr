#!/bin/bash

set -ex

arachne_pnr=../../bin/arachne-pnr

valgrind $arachne_pnr sb_up3down5.blif -o sb_up3down5_valgrind.txt
icepack sb_up3down5_valgrind.txt sb_up3down5_valgrind.bin
