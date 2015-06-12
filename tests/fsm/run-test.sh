#!/bin/bash
set -ex

: ${ARACHNE_PNR:=../../arachne-pnr}
: ${ICEBOX_BLOG:=icebox_vlog}
: ${ICEPACK:=icepack}
: ${PYTHON:=python}
: ${YOSYS:=yosys}

rm -rf temp
mkdir temp
$PYTHON generate.py

for i in temp/uut_?????.v; do
    pf=${i%.*}
    base=${pf##*/}
    $YOSYS -q -p "synth_ice40 -blif ${pf}_gate.blif" ${pf}.v
    rm -f ${pf}_gate.chip.txt # don't pick up stale copy
    $ARACHNE_PNR -w ${pf}_gate.pcf -V ${pf}_pp.v -o ${pf}_gate.chip.txt ${pf}_gate.blif > ${pf}.pnr-log 2>&1 \
      || grep -q 'failed to route' ${pf}.pnr-log
    if [ -f ${pf}_gate.chip.txt ]; then
	# check pp
	$YOSYS -q -l ${pf}_pp.log ${pf}_pp.ys
	grep -q 'SAT proof finished - no model found: SUCCESS!' ${pf}_pp.log \
	    || grep -q 'Interrupted SAT solver: TIMEOUT!' ${pf}_pp.log
	
	# check bitstream
	$ICEPACK ${pf}_gate.chip.txt ${pf}_gate.bin
	# icebox_explain ${pf}_gate.chip.txt > ${pf}_gate.ex
	$ICEBOX_VLOG -n ${base} -p ${pf}_gate.pcf ${pf}_gate.chip.txt > ${pf}_gate.v
	$YOSYS -q -l ${pf}.log ${pf}.ys
	grep -q 'SAT proof finished - no model found: SUCCESS!' ${pf}.log \
	    || grep -q 'Interrupted SAT solver: TIMEOUT!' ${pf}.log
    fi
done
echo OK.
