#!/bin/bash

set -ex

rm -rf build

mkdir build
cd build

cmake -DCMAKE_BUILD_TYPE=Debug ..
make VERBOSE=1
make VERBOSE=1 ARGS="-L fast -V" test

cd ..
