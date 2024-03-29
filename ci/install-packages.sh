#! /bin/bash

# For g++-9
sudo add-apt-repository ppa:ubuntu-toolchain-r/test -y

# For clang-8
sudo echo "deb http://apt.llvm.org/xenial/ llvm-toolchain-xenial-8 main" >> /etc/apt/sources.list
wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key|sudo apt-key add -

# We need openssl 1.1
sudo add-apt-repository ppa:hola-launchpad/openssl -y

# For googltest
sudo add-apt-repository ppa:neon1ks/xenial -y

sudo apt-get update

# NOTE ocaml-nox changes to ocamlbuild in bionic
sudo apt-get install -y --no-install-recommends \
    g++-9 sudo ca-certificates git pkg-config \
    build-essential ocaml automake autoconf libtool wget python ocaml-nox libssl-dev \
    libgmp3-dev libgflags-dev libgoogle-glog-dev google-mock libgtest-dev googletest \
    pkg-config libboost-program-options-dev libboost-python-dev unzip ninja-build clang-tidy-8 \
    python3-pip python3-dev python3 python3-setuptools python3-wheel psmisc doxygen graphviz cmake

