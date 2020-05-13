#!/bin/bash -x

cmake --build ../build 

clang++ hello.cpp -c -emit-llvm
clang++ ../runtime/MemoryHooks.cpp -c -emit-llvm

llvm-link hello.bc MemoryHooks.bc > link.bc
llvm-dis link.bc

opt -load ../build/lib/LLVMAasanInstrument.so -memory-instrument < link.bc > opt.bc

clang++ opt.bc
llvm-dis opt.bc
./a.out > out

