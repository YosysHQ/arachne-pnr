#!/bin/bash

set -ex

rm -rf build

function build_test {
    cc=$1
    cxx=$2
    build=$3
    target=$4
    dir=${cc}-${debug}
    
    mkdir build/$dir
    cd build/$dir
    cmake -DCMAKE_C_COMPILER=$cc -DCMAKE_CXX_COMPILER=$cxx -DCMAKE_BUILD_TYPE=$build ../..
    make VERBOSE=1 $target
    cd ../..
}

build_test clang clang++ Debug simpletest
build_test clang clang++ Release simpletest
build_test clang clang++ RelWithDebug simpletest

build_test gcc g++ Debug simpletest
build_test gcc g++ Release simpletest
build_test gcc g++ RelWithDebug test # build default
