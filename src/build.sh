#! /bin/bash

PASSNAME="prediction_pass"
source vars.sh

echo 'LLVM_DIR = [' $LLVM_DIR ']'

if [[ ! -d build ]]; then
    mkdir build
fi

cd build

cmake -GNinja -DLT_LLVM_INSTALL_DIR=$LLVM_DIR ..
ninja
