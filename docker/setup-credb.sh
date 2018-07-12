#!/bin/bash

set -e
set -x

source docker/paths.sh

export CC=clang-6.0
export CXX=clang++-6.0

rm -rf build # just in case we did copy the build directory
meson build --prefix=$HOME/local
cd build
meson configure -Dsgx_mode=$sgx_mode -Dbuildtype=$buildtype -Dalways_page=true
ninja
meson configure -Dsgx_mode=$sgx_mode -Dbuildtype=$buildtype -Dalways_page=true # for some reason, fake_enclave need to build twice
ninja
ninja install
