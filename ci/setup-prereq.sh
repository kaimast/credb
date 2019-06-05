#! /usr/bin/bash

export LD_LIBRARY_PATH=$HOME/local/lib:$HOME/local/lib/x86_64-linux-gnu:/usr/local/lib:/usr/local/lib/x86_64-linux-gnu
export LIBRARY_PATH=$HOME/local/lib:$HOME/local/lib/x86_64-linux-gnu:/usr/local/lib:/usr/local/lib/x86_64-linux-gnu

export CC=gcc-9
export CXX=g++-9

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


