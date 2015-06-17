#!/bin/bash

set -ex

rm -rf build

function build_test {
    cc=$1
    cxx=$2
    build=$3
    args="$4"
    dir=${cc}-${build}
    
    mkdir -p build/$dir
    cd build/$dir
    cmake -DCMAKE_C_COMPILER=$cc -DCMAKE_CXX_COMPILER=$cxx -DCMAKE_BUILD_TYPE=$build ../..
    make VERBOSE=1
    make VERBOSE=1 ARGS="-V $args" test    
    cd ../..
}

build_test clang clang++ Debug "-L fast"
build_test clang clang++ Release "-L fast"
build_test clang clang++ RelWithDebug "-L fast"

build_test gcc g++ Debug "-L fast"
build_test gcc g++ Release "-L fast"
build_test gcc g++ RelWithDebug "" # default
