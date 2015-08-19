#!/bin/bash

set -ex

arachne_pnr="../../bin/arachne-pnr"
devices='1k 8k'
: ${ICEBOX:=/usr/local/share/icebox}

rm -f txt.sum

for d in $devices; do
    rm -rf $d
    mkdir $d
    
    $arachne_pnr -d $d -c $ICEBOX/chipdb-$d.txt --write-binary-chipdb $d/chipdb-$d.bin
    $arachne_pnr -d $d -c $d/chipdb-$d.bin --write-binary-chipdb $d/chipdb2-$d.bin
    cmp $d/chipdb-$d.bin $d/chipdb2-$d.bin
    
    # sb_up3down5.blif
    $arachne_pnr -d $d sb_up3down5.blif -o $d/sb_up3down5.txt
    shasum $d/sb_up3down5.txt >> txt.sum
    icepack $d/sb_up3down5.txt $d/sb_up3down5.bin
    
    $arachne_pnr -d $d -l sb_up3down5.blif -o $d/sb_up3down5_l.txt
    shasum $d/sb_up3down5_l.txt >> txt.sum
    icepack $d/sb_up3down5_l.txt $d/sb_up3down5_l.bin
    
    $arachne_pnr -d $d sb_up3down5.blif -B $d/sb_up3down5_packed.blif -o $d/sb_up3down5.txt
    $arachne_pnr -d $d $d/sb_up3down5_packed.blif -o $d/sb_up3down5_packed.txt
    shasum $d/sb_up3down5_packed.txt >> txt.sum
    icepack $d/sb_up3down5_packed.txt $d/sb_up3down5_packed.bin
    
    $arachne_pnr -d $d carry.blif -o $d/carry.txt
    shasum $d/carry.txt >> txt.sum
    icepack $d/carry.txt $d/carry.bin

    $arachne_pnr -d $d bram.blif -o $d/bram.txt
    shasum $d/bram.txt >> txt.sum
    icepack $d/bram.txt $d/bram.bin
done
