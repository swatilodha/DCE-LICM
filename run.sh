#! /bin/sh
export TYPE=$1

opt -enable-new-pm=0 -load ./$TYPE.so -$TYPE ./tests/$TYPE-test-m2r.bc -o /dev/null

