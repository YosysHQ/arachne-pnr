#!/bin/bash

set -ex

make clean
make CC=clang CXX=clang++ OPTDEBUGFLAGS='-O0 -fno-inline -g' simpletest

make clean
make CC=gcc CXX=g++ OPTDEBUGFLAGS='-O0 -fno-inline -g' simpletest

make clean
make CC=clang CXX=clang++ OPTDEBUGFLAGS='-O2' simpletest

make clean
make CC=gcc CXX=g++ OPTDEBUGFLAGS='-O2' simpletest

make clean
make CC=clang CXX=clang++ OPTDEBUGFLAGS='-O2 -DNDEBUG' simpletest

make clean
make CC=gcc CXX=g++ OPTDEBUGFLAGS='-O2 -DNDEBUG' simpletest

# full test with default options
make clean
make test
make testvg

make clean
