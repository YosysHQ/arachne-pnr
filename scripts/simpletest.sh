#!/bin/bash

set -ex

if [ ! -d build ]; then
    echo "build: No such directory"
    exit 1
fi

cd build

cmake -DCMAKE_BUILD_TYPE=Debug ..
make VERBOSE=1
make VERBOSE=1 ARGS="-L fast -V" test

cd ..
