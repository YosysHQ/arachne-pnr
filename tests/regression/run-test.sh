#!/bin/bash

set -ex

arachne_pnr=../../bin/arachne-pnr

for d in 1k 8k; do
    rm -rf $d
    mkdir $d
    
    $arachne_pnr -d $d bram1.blif -o $d/bram1.txt
    icepack $d/bram1.txt $d/bram1.bin
    
    $arachne_pnr -d $d carry_pack_fail1.blif -o $d/carry_pack_fail1.txt
    icepack $d/carry_pack_fail1.txt $d/carry_pack_fail1.bin

    $arachne_pnr -d $d test1.blif -o $d/test1.txt
    icepack $d/test1.txt $d/test1.bin
    
    $arachne_pnr -d $d test2.blif -o $d/test2.txt
    icepack $d/test2.txt $d/test2.bin
    
    $arachne_pnr -d $d -w $d/carry_route_fail1.pcf carry_route_fail1.blif -o $d/carry_route_fail1.txt
    icebox_vlog -n test -p $d/carry_route_fail1.pcf $d/carry_route_fail1.txt > $d/carry_route_fail1.chip.v
    yosys -q -l $d/carry_route_fail1.log -p "read_verilog carry_route_fail1.v; rename test gold; read_verilog $d/carry_route_fail1.chip.v; rename test gate; hierarchy; proc;; miter -equiv -flatten -ignore_gold_x -make_outputs -make_outcmp gold gate miter; sat -verify-no-timeout -timeout 60 -prove trigger 0 -show-inputs -show-outputs miter"
    
    $arachne_pnr -d $d j1a_gb_fail.blif -o $d/j1a_gb_fail.txt
    icepack $d/j1a_gb_fail.txt $d/j1a_gb_fail.bin
    
    $arachne_pnr -d $d c3demo.blif -o $d/c3demo.txt
    icepack $d/c3demo.txt $d/c3demo.bin
done

$arachne_pnr -d 8k -p pin_type_fail.pcf pin_type_fail.blif -o /dev/null
