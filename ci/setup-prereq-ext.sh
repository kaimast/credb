#! /bin/bash

WORKDIR=$HOME/prereq
INSTALL_DIR=$HOME/local
PY_VERSION=python3.5

BUILDTYPE=release
export CC=gcc-9
export CXX=g++-9

export LD_LIBRARY_PATH=$HOME/local/lib:$HOME/local/lib/x86_64-linux-gnu:/usr/local/lib:/usr/local/lib/x86_64-linux-gnu
export LIBRARY_PATH=$HOME/local/lib:$HOME/local/lib/x86_64-linux-gnu:/usr/local/lib:/usr/local/lib/x86_64-linux-gnu
export PYTHONPATH=${INSTALL_DIR}/lib/${PY_VERSION}/site-packages:${INSTALL_DIR}/lib/${PY_VERSION}/dist-packages

ssh-keyscan github.com >> ~/.ssh/known_hosts

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

mkdir -p ${INSTALL_DIR}/lib/${PY_VERSION}/site-packages
mkdir -p ${INSTALL_DIR}/lib/${PY_VERSION}/dist-packages

#
cd $WORKDIR
if clone-repo "meson" "https://github.com/mesonbuild/meson.git"; then
    cd meson
    git checkout 0.50.1
    python3 ./setup.py install --prefix=$INSTALL_DIR
fi

cd $WORKDIR
if clone-repo "botan" "https://github.com/randombit/botan.git"; then
    cd botan
    echo "Building botan"
    git checkout release-2
    python ./configure.py --with-openssl --prefix=$INSTALL_DIR
    make -j10
    make install
fi

# sgxsdk
git clone https://github.com/01org/linux-sgx.git > /dev/null 2>&1
cd linux-sgx
git checkout sgx_2.5
./download_prebuilt.sh > /dev/null 2>&1
make sdk_install_pkg > /dev/null 2>&1
printf "no\n/opt/intel\n" | sudo $(ls linux/installer/bin/sgx_linux_x64_sdk_*.bin) > /dev/null 2>&1
cd ..
rm -rf linux-sgx

cd $WORKDIR
if clone-repo "libpypa" "git@github.com:vinzenz/libpypa.git"; then
    cd libpypa
    echo "Building pypa"
    ./autogen.sh
    ./configure --prefix=$INSTALL_DIR
    make -j10
    make install
fi

cd $WORKDIR
if clone-repo "pybind11" "https://github.com/pybind/pybind11.git"; then
    cd pybind11
    echo "Building pybind11"
    git checkout v2.2
    cmake -DPYBIND11_TEST=OFF -DPYBIND11_PYTHON_VERSION=3.5 -DCMAKE_INSTALL_PREFIX=$INSTALL_DIR
    make
    make install
fi
