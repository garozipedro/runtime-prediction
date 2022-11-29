#! /bin/bash

PASSNAME="prediction_pass"
TEST=permutation

source vars.sh
echo 'LLVM_DIR = [' $LLVM_DIR ']'

echo '* Compiling test file...'
$LLVM_DIR/bin/clang -emit-llvm -S -O0 test_progs/${TEST}.c -o ./${TEST}.ll
echo '* Running opt on test...'
$LLVM_DIR/bin/opt -load-pass-plugin build/lib${PASSNAME}.so -passes="${PASSNAME}" -disable-output ${TEST}.ll --prediction-cost-kind 'latency'

