#! /bin/bash

set -e
set -x
export LD_LIBRARY_PATH=$HOME/local/lib:$HOME/local/lib/x86_64-linux-gnu:/usr/local/lib:/usr/local/lib/x86_64-linux-gnu
export LIBRARY_PATH=$HOME/local/lib:$HOME/local/lib/x86_64-linux-gnu:/usr/local/lib:/usr/local/lib/x86_64-linux-gnu
export PYTHONPATH=${HOME}/local/lib/python3.6/site-packages

if [[ "$sgx_mode" == "simulation" ]]; then
    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/opt/intel/sgxsdk/lib64
    export LIBRARY_PATH=$LIBRARY_PATH:/opt/intel/sgxsdk/lib64
fi

export TIDY=clang-tidy

function clang_tidy_works {
    if ! hash $TIDY 2> /dev/null; then
        return 1
    fi

    return 0
}

if ! clang_tidy_works; then
    echo "Clang Tidy isn't available!"
    exit 0
fi

cd build
ninja tidy
