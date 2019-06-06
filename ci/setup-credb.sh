#! /bin/bash

export CC=gcc-9
export CXX=g++-9

WORKDIR=$HOME/prereq
INSTALL_DIR=$HOME/local
PY_VERSION=python3.5

export LD_LIBRARY_PATH=$HOME/local/lib:$HOME/local/lib/x86_64-linux-gnu:/usr/local/lib:/usr/local/lib/x86_64-linux-gnu
export LIBRARY_PATH=$HOME/local/lib:$HOME/local/lib/x86_64-linux-gnu:/usr/local/lib:/usr/local/lib/x86_64-linux-gnu
export PYTHONPATH=${INSTALL_DIR}/lib/${PY_VERSION}/site-packages:${INSTALL_DIR}/lib/${PY_VERSION}/dist-packages
export PATH=${PATH}:${INSTALL_DIR}/bin

SGX_MODE=simulation
BUILDTYPE=release

meson build --prefix=$HOME/local
cd build
meson configure -Dsgx_mode=$SGX_MODE -Dbuildtype=$BUILDTYPE -Dalways_page=true
ninja
meson configure -Dsgx_mode=$SGX_MODE -Dbuildtype=$BUILDTYPE -Dalways_page=true
ninja
ninja install

ninja tidy
