# Arachne-pnr

## Updates

2015-08-06: Interface change: Default seed is 1, can be randomized with `-r` option.

2015-07-18: New version.  Release notes:
* IceStorm and arachne-pnr now support the iCE40LP/HX8K
* huge speed improvements (~50x)
* arachne-pnr now prints the random seed on each run.  The seed can be set with the `-s` option.  With the same seed, arachne-pnr should be deterministic across platforms and C++ compilers.

## What is arachne-pnr?

Arachne-pnr implements the place and route step of the hardware
compilation process for FPGAs.  It accepts as input a
technology-mapped netlist in BLIF format, as output by the
[Yosys](http://www.clifford.at/yosys/) [0] synthesis suite for
example.  It currently targets the Lattice Semiconductor
[iCE40](http://www.latticesemi.com/iCE40) family of FPGAs [1].  Its
output is a textual bitstream representation for assembly by the
[IceStorm](http://www.clifford.at/icestorm/) [2] `icepack` command.
The output of `icepack` is a binary bitstream which can be uploaded to
a hardware device.

Together, Yosys, arachne-pnr and IceStorm provide an fully open-source
Verilog-to-bistream tool chain for iCE40 1K and 8K FPGA development.

## Warning!

This is experimental software!  It might have bugs that cause it to
produce bitstreams which could damage your FPGA!  So when you buy an
evaluation board, get a few.

We have done extensive verification-based testing (see `tests/`), but
so far limited hardware-based testing.  This will change.

## Status

Arachne-pnr uses a simulated annealing-based algorithm for placement
and a multi-pass congestion-based router.  Arachne-pnr supports all
features documented by IceStorm, although the Block RAM has not been
extensively tested.  This should include everything on the chip except
PLLs and timing information.  Support for PLLs is in the works.  We
hope to support timing information as soon as it is documented by the
IceStorm project.

## Installing

Arachne-pnr is written in C++11.  It has been tested under OSX and
Linux with LLVM/Clang and GCC.  It should work on Windows, perhaps
with a little work.  (Patches welcome.)  It depends on IceStorm.  You
should also install Yosys to synthesize designs.

To install, just go to the arachne-pnr directory and run

```
$ make && sudo make install
```

## Invoking/Example

Lattice has several low-cost breakout boards:
[iCEstick](http://latticesemi.com/iCEstick) for the 1K [3] and
[iCE40-HX8K Breakout
Board](http://www.latticesemi.com/en/Products/DevelopmentBoardsAndKits/iCE40HX8KBreakoutBoard.aspx)
[4] for the 8K.

There is a simple example for the iCEstick evaluation board in
`example/rot` which generates a rotating pattern on the user LEDs.
The Verilog input is in `rot.v` and the physical (pin) constraints are
in `rot.pcf`.  To build it, just run `make`, which executes the
following commands:

```
yosys -q -p "synth_ice40 -blif rot.blif" rot.v
../../bin/arachne-pnr -p rot.pcf rot.blif -o rot.txt
icepack rot.txt rot.bin
```

You can use the Lattice tools or `iceprog` from IceStorm to upload
`rot.bin` onto the board.

## Feedback

Feedback, feature requests, bug reports and patches welcome.  Please
email the author, Cotton Seed <cotton@alum.mit.edu>, or submit a issue
on Github.

## Acknowledgements

Arachne-pnr would not have been possible without Clifford Wolf and
Mathias Lasser's IceStorm project to reverse-engineer the iCE40 FPGAs
and build supporting tools.  Also, it would not be useful without
Clifford Wolf's Yosys project to synthesize and technology map
designs.  Thanks to contributors Larry Doolittle, jhol, laanwj,
Clifford Wolf (cliffordwolf) and zeldin.

## References

[0] http://www.clifford.at/yosys/

[1] http://www.latticesemi.com/iCE40

[2] http://www.clifford.at/icestorm/

[3] http://latticesemi.com/iCEstick

[4] http://www.latticesemi.com/en/Products/DevelopmentBoardsAndKits/iCE40HX8KBreakoutBoard.aspx
