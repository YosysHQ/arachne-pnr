#!/bin/bash
set -ex

arachne_pnr=../../bin/arachne-pnr

rm -rf temp
mkdir temp
python generate.py

for i in temp/uut_?????.v; do
    pf=${i%.*}
    base=${pf##*/}
    yosys -q -p "synth_ice40 -blif ${pf}_gate.blif" ${pf}.v
    rm -f ${pf}_gate.chip.txt # don't pick up stale copy
    $arachne_pnr -w ${pf}_gate.pcf -V ${pf}_pp.v -o ${pf}_gate.chip.txt ${pf}_gate.blif > ${pf}.pnr-log 2>&1 \
      || grep -q 'failed to route' ${pf}.pnr-log
    if [ -f ${pf}_gate.chip.txt ]; then
	# check pp
	yosys -q -l ${pf}_pp.log ${pf}_pp.ys
	grep -q 'SAT proof finished - no model found: SUCCESS!' ${pf}_pp.log \
	    || grep -q 'Interrupted SAT solver: TIMEOUT!' ${pf}_pp.log
	
	# check bitstream
	icepack ${pf}_gate.chip.txt ${pf}_gate.bin
	# icebox_explain ${pf}_gate.chip.txt > ${pf}_gate.ex
	icebox_vlog -n ${base} -p ${pf}_gate.pcf ${pf}_gate.chip.txt > ${pf}_gate.v
	yosys -q -l ${pf}.log ${pf}.ys
	grep -q 'SAT proof finished - no model found: SUCCESS!' ${pf}.log \
	    || grep -q 'Interrupted SAT solver: TIMEOUT!' ${pf}.log
    fi
done
echo OK.
