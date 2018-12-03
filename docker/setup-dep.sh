#!/bin/bash
# keep sudo as this may be useful in non-docker environment
set -e
set -x

export LD_LIBRARY_PATH=$HOME/local/lib:$HOME/local/lib/x86_64-linux-gnu:/usr/local/lib:/usr/local/lib/x86_64-linux-gnu
export LIBRARY_PATH=$HOME/local/lib:$HOME/local/lib/x86_64-linux-gnu:/usr/local/lib:/usr/local/lib/x86_64-linux-gnu

mkdir -p $HOME/local/{bin,include,lib}

# packages
apt-get update > /dev/null 2>&1
apt-get install -y --no-install-recommends \
    g++-8 sudo ca-certificates git pkg-config \
    build-essential ocaml automake autoconf libtool wget python ocamlbuild libssl-dev \
    libgmp3-dev libgflags-dev libgoogle-glog-dev google-mock googletest libgtest-dev \
    pkg-config libboost-program-options-dev libboost-python-dev unzip ninja-build \
    clang-7.0 clang-tidy-7.0 clang-format \
    python3-pip python3-dev python3 meson python3-setuptools python3-wheel psmisc doxygen graphviz libbotan-2-dev cmake > /dev/null 2>&1

#SGX SDK doesn't support clang yet
export CC=gcc-8
export CXX=g++-8

# sgxsdk
git clone https://github.com/01org/linux-sgx.git > /dev/null 2>&1
cd linux-sgx
git checkout sgx_2.3.1
./download_prebuilt.sh > /dev/null 2>&1
make sdk_install_pkg > /dev/null 2>&1
printf "no\n/opt/intel\n" | sudo $(ls linux/installer/bin/sgx_linux_x64_sdk_*.bin) > /dev/null 2>&1
cd ..
rm -rf linux-sgx

export CC=clang-7
export CXX=clang++-7

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

# bitstream
git clone https://github.com/kaimast/bitstream.git
cd bitstream
#meson build --prefix=$HOME/local/
meson build --prefix=/usr/local/
cd build
ninja install
cd ../..
rm -rf bitstream

# yael
git clone https://github.com/kaimast/yael.git
cd yael
meson build --prefix=$HOME/local/
cd build
meson configure -Dbuildtype=release
ninja
ninja install
cd ../..
rm -rf yael

# libdocument
git clone https://github.com/kaimast/libdocument.git
cd libdocument
meson build --prefix=$HOME/local/
cd build
meson configure -Dbuildtype=release
ninja
ninja install
cd ../..
rm -rf libdocument

# cowlang
git clone https://github.com/kaimast/cowlang.git
cd cowlang
meson build --prefix=$HOME/local/
cd build
meson configure -Dbuildtype=release
ninja
ninja install
cd ../..
rm -rf cowlang

# cleanup
sudo apt-get purge -y ocaml ocamlbuild
sudo apt-get autoremove -y
sudo rm -rf /var/lib/apt/lists/*
