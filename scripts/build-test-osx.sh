#!/bin/bash

set -ex

rm -rf build

function build_test {
    build="$1"
    args="$2"
    
    mkdir -p build/$build
    cd build/$build
    cmake -DCMAKE_BUILD_TYPE=$build ../..
    make VERBOSE=1
    make VERBOSE=1 ARGS="-V $args" test
    cd ../..
}

build_test Debug "-L fast"
build_test Release "-L fast"
build_test RelWithDebug "" # default
