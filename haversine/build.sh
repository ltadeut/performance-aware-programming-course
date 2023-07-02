#!/usr/bin/env bash

set -x

mkdir -p build

pushd build

clang -std=c17 -O2 ../gen.c -o GenerateRandomHaversineData
clang -std=c17 -g -O0 ../processor.c -o ComputeHaversineAverage

popd
