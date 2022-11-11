#! /bin/bash

PASSNAME="prediction_pass"

if [[ ! -d build ]]; then
    mkdir build
fi
cd build
#export
LLVM_DIR=/usr/lib/llvm-14
cmake -GNinja -DLT_LLVM_INSTALL_DIR=$LLVM_DIR ..
ninja


echo ''
echo '* Compiling test file...'
TEST=permutation
$LLVM_DIR/bin/clang -emit-llvm -S -O0 ../test_progs/${TEST}.c -o ./${TEST}.ll
echo '* Running opt on test...'
$LLVM_DIR/bin/opt -load-pass-plugin ./lib${PASSNAME}.so -passes="${PASSNAME}" -disable-output ${TEST}.ll

