#! /usr/bin/bash

export LD_LIBRARY_PATH=$HOME/local/lib:$HOME/local/lib/x86_64-linux-gnu:/usr/local/lib:/usr/local/lib/x86_64-linux-gnu
export LIBRARY_PATH=$HOME/local/lib:$HOME/local/lib/x86_64-linux-gnu:/usr/local/lib:/usr/local/lib/x86_64-linux-gnu

export CC=gcc-9
export CXX=g++-9

# sgxsdk
git clone https://github.com/01org/linux-sgx.git > /dev/null 2>&1
cd linux-sgx
git checkout sgx_2.5
./download_prebuilt.sh > /dev/null 2>&1
make sdk_install_pkg > /dev/null 2>&1
printf "no\n/opt/intel\n" | sudo $(ls linux/installer/bin/sgx_linux_x64_sdk_*.bin) > /dev/null 2>&1
cd ..
rm -rf linux-sgx

# libpypa
git clone https://github.com/vinzenz/libpypa.git > /dev/null 2>&1
cd libpypa
./autogen.sh > /dev/null 2>&1
./configure --quiet --prefix=$HOME/local > /dev/null 2>&1
make -j2 > /dev/null 2>&1
make install > /dev/null 2>&1
cd ..
rm -rf libpypa

# pybind11
git clone https://github.com/pybind/pybind11.git
cd pybind11
git checkout v2.2
cmake -DPYBIND11_TEST=OFF -DPYBIND11_PYTHON_VERSION=3.5 -DCMAKE_INSTALL_PREFIX=$INSTALL_DIR
make
make install
cd ..
rm -rf pybind11


