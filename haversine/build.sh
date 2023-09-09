#!/usr/bin/env bash

set -x

mkdir -p build

pushd build

clang -std=c17 -O2 ../gen.c -o GenerateRandomHaversineData
clang++ -g -O0 ../processor.cpp -o ComputeHaversineAverage
clang++ -g -O0 ../repetition_tester.cpp -o Test

popd
