#! /bin/bash

PASSNAME="prediction_pass"

if [[ $1 ]]; then
    LLVM_DIR=$1
else
    LLVM_DIR=/usr/lib/llvm-15
fi

echo 'LLVM_DIR = [' $LLVM_DIR ']'

if [[ ! -d build ]]; then
    mkdir build
fi

cd build

cmake -GNinja -DLT_LLVM_INSTALL_DIR=$LLVM_DIR ..
ninja
