#!/bin/bash

set -ex

make clean
make OPTDEBUGFLAGS='-O0 -fno-inline -g' simpletest

make clean
make OPTDEBUGFLAGS='-O2' simpletest

make clean
make OPTDEBUGFLAGS='-O2 -DNDEBUG' simpletest

make clean
