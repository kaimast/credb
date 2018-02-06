#!/bin/bash

set -e
set -x
export LD_LIBRARY_PATH=$HOME/local/lib:$HOME/local/lib/x86_64-linux-gnu:/usr/local/lib:/usr/local/lib/x86_64-linux-gnu
export LIBRARY_PATH=$HOME/local/lib:$HOME/local/lib/x86_64-linux-gnu:/usr/local/lib:/usr/local/lib/x86_64-linux-gnu
export PYTHONPATH=${HOME}/local/lib/python3.6/site-packages

if [[ "$sgx_mode" == "simulation" ]]; then
    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/opt/intel/sgxsdk/lib64
    export LIBRARY_PATH=$LIBRARY_PATH:/opt/intel/sgxsdk/lib64
fi

export CC=gcc-7
export CXX=g++-7

rm -rf build # just in case we did copy the build directory
meson build --prefix=$HOME/local
cd build
meson configure -Dsgx_mode=$sgx_mode -Dbuildtype=$buildtype -Dalways_page=true
ninja
meson configure -Dsgx_mode=$sgx_mode -Dbuildtype=$buildtype -Dalways_page=true # for some reason, fake_enclave need to build twice
ninja
ninja install
