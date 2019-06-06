#! /usr/bin/bash

WORKDIR=$HOME/prereq
INSTALL_DIR=$HOME/local
PY_VERSION=python3.5

BUILDTYPE=release
export CC=gcc-9
export CXX=g++-9

export LD_LIBRARY_PATH=$HOME/local/lib:$HOME/local/lib/x86_64-linux-gnu:/usr/local/lib:/usr/local/lib/x86_64-linux-gnu
export LIBRARY_PATH=$HOME/local/lib:$HOME/local/lib/x86_64-linux-gnu:/usr/local/lib:/usr/local/lib/x86_64-linux-gnu
export PYTHONPATH=${INSTALL_DIR}/lib/${PY_VERSION}/site-packages:${INSTALL_DIR}/lib/${PY_VERSION}/dist-packages
export PATH=${PATH}:${INSTALL_DIR}/bin

function clone-repo() {
    dir=$1
    url=$2

    if [ -d $dir ]; then
        return 1
    else
        git clone $url $dir
        return 0
    fi
}

cd $WORKDIR
if clone-repo "bitstream" "https://github.com/kaimast/bitstream.git"; then
    cd bitstream
    meson build --prefix=/usr/local/
    cd build
    ninja install
fi

cd $WORKDIR
if clone-repo "yael" "https://github.com/kaimast/yael.git"; then
    cd yael
    meson build --prefix=$HOME/local/
    cd build
    meson configure -Dbuildtype=release
    ninja
    ninja install
fi

cd $WORKDIR
if clone-repo "libdocument" "https://github.com/kaimast/libdocument.git"; then
    cd libdocument
    meson build --prefix=$HOME/local/
    cd build
    meson configure -Dbuildtype=release
    ninja
    ninja install
fi

cd $WORKDIR
if clone-repo "cowlang" "https://github.com/kaimast/cowlang.git"; then
    cd cowlang
    meson build --prefix=$HOME/local/
    cd build
    meson configure -Dbuildtype=release
    ninja
    ninja install
fi
