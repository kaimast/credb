export LD_LIBRARY_PATH=$HOME/local/lib:$HOME/local/lib/x86_64-linux-gnu:/usr/local/lib:/usr/local/lib/x86_64-linux-gnu
export LIBRARY_PATH=$HOME/local/lib:$HOME/local/lib/x86_64-linux-gnu:/usr/local/lib:/usr/local/lib/x86_64-linux-gnu

if [[ "$sgx_mode" == "simulation" ]]; then
    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/opt/intel/sgxsdk/lib64
    export LIBRARY_PATH=$LIBRARY_PATH:/opt/intel/sgxsdk/lib64
fi
