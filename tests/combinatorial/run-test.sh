#!/bin/bash
set -ex

arachne_pnr=../../bin/arachne-pnr

rm -rf temp
mkdir temp
python generate.py

for d in 1k 8k; do
    rm -rf ${d}
    mkdir ${d}
    for i in temp/uut_?????.v; do
        pf=${i%.*}  # temp/uut_00000
        base=${pf##*/} # uut_00000
        dpf=${d}/${base} # 1k/uut_00000
        yosys -q -p "synth_ice40 -blif ${dpf}_gate.blif" ${pf}.v
        rm -f ${dpf}_gate.chip.txt # don't pick up stale copy
        $arachne_pnr -d ${d} -w ${dpf}_gate.pcf -V ${dpf}_pp.v -o ${dpf}_gate.chip.txt ${dpf}_gate.blif > ${dpf}.pnr-log 2>&1 \
            || grep -q 'failed to route' ${dpf}.pnr-log
        if [ -f ${dpf}_gate.chip.txt ]; then
            # check pp
            yosys -q -l ${dpf}_pp.log -p "read_verilog +/ice40/cells_sim.v ${dpf}_pp.v; script ${pf}_pp.ys"
            grep -q 'SAT proof finished - no model found: SUCCESS!' ${dpf}_pp.log \
                || grep -q 'Interrupted SAT solver: TIMEOUT!' ${dpf}_pp.log
            
            # check bitstream
            icepack ${dpf}_gate.chip.txt ${dpf}_gate.bin
            # icebox_explain ${dpf}_gate.chip.txt > ${dpf}_gate.ex
            icebox_vlog -n ${base} -p ${dpf}_gate.pcf ${dpf}_gate.chip.txt > ${dpf}_gate.v
            yosys -q -l ${dpf}.log -p "read_verilog ${dpf}_gate.v; script ${pf}.ys"
            grep -q 'SAT proof finished - no model found: SUCCESS!' ${dpf}.log \
                || grep -q 'Interrupted SAT solver: TIMEOUT!' ${dpf}.log
        fi
    done
done
echo OK.
