#!/bin/bash

OPENSSL_CFGSCPT=$(pwd)/libs/openssl/Configure

# Build script for openssl with emscripten because this is really annoying

mkdir openssl_build
cd openssl_build
emmake perl $OPENSSL_CFGSCPT -no-asm no-hw no-engine -static --prefix=$(pwd)/libs/install/web linux-generic64
sed -i 's/CROSS_COMPILE=.*/CROSS_COMPILE=/g' Makefile
emmake make -j $(nproc)
emmake make install
