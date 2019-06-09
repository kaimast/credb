#! /bin/bash

export CC=gcc-9
export CXX=g++-9

INSTALL_DIR=$HOME/local
PY_VERSION=python3.5
SGX_DIR=${INSTALL_DIR}/intel
SGX_SDK_DIR=${SGX_DIR}/sgxsdk
SGX_MODE=simulation
BUILDTYPE=release

export LD_LIBRARY_PATH=$HOME/local/lib:$HOME/local/lib/x86_64-linux-gnu:/usr/local/lib:/usr/local/lib/x86_64-linux-gnu:${SGX_SDK_DIR}/lib64
export LIBRARY_PATH=$HOME/local/lib:$HOME/local/lib/x86_64-linux-gnu:/usr/local/lib:/usr/local/lib/x86_64-linux-gnu:${SGX_SDK_DIR}/lib64
export PYTHONPATH=${INSTALL_DIR}/lib/${PY_VERSION}/site-packages:${INSTALL_DIR}/lib/${PY_VERSION}/dist-packages
export PATH=${PATH}:${INSTALL_DIR}/bin

meson build -Dsgx_mode=$SGX_MODE -Dbuildtype=$BUILDTYPE -Dalways_page=true -Dsgx_sdk_dir=${SGX_SDK_DIR} --prefix=$HOME/local
cd build
ninja -v
ninja install

ninja tidy

./credb testserver &
sleep 1

./credb-test
killall credb
