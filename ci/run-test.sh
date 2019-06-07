#! /bin/bash

INSTALL_DIR=$HOME/local
PY_VERSION=python3.5

export LD_LIBRARY_PATH=$HOME/local/lib:$HOME/local/lib/x86_64-linux-gnu:/usr/local/lib:/usr/local/lib/x86_64-linux-gnu
export LIBRARY_PATH=$HOME/local/lib:$HOME/local/lib/x86_64-linux-gnu:/usr/local/lib:/usr/local/lib/x86_64-linux-gnu
export PYTHONPATH=${HOME}/local/lib/python3.6/site-packages:${HOME}/local/lib/python3.6/dist-packages

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/opt/intel/sgxsdk/lib64
export LIBRARY_PATH=$LIBRARY_PATH:/opt/intel/sgxsdk/lib64

unit_test() {
    ./credb testserver > /dev/null 2>&1 &
    sleep 10
    ./credb-test
    killall -9 credb
}

concurrent_transactions() {
    ../test/concurrent_transactions.py
}

multi_update() {
    ../test/multi_update.py --num_updates=10000 --num_clients=20
}

multi_get() {
    ../test/multi_get.py --num_gets=10000 --num_clients=20
}

multi_put() {
    ../test/multi_put.py --num_puts=10000 --num_clients=20
}

call_program() {
    ../test/call_programs.py
    ../test/concurrent_remote_call_tx.py
    ../test/concurrent_call_programs.py
    ../test/concurrent_remote_call.py
}

bank_test() {
    ../test/bank_test.py
}

debug_snapshot() {
    ./credb testserver > /dev/null 2>&1 &
    sleep 10
    ../test/debug_snapshot.py put --create_index
    ../test/debug_snapshot.py dump
    killall -9 credb

    ./credb testserver > /dev/null 2>&1 &
    sleep 10
    ../test/debug_snapshot.py load
    ../test/debug_snapshot.py find
    killall -9 credb
}

multi_downstream() {
    ../test/multi_downstream.py --num_objs=5000 --num_clients=8
}

witness() {
    ./credb testserver > /dev/null 2>&1 &
    sleep 10
    ../test/witness.py --no_server --server_port 5042
    killall -9 credb
}

cd build

case $run_test in
    unit_test)
        unit_test
        ;;
    multi_get)
        multi_get
        ;;
    multi_update)
        multi_update
        ;;
    multi_put)
        multi_put
        ;;
    call_program)
        call_program
        ;;
    bank_test)
        bank_test
        ;;
    concurrent_transactions)
        concurrent_transactions
        ;;
    multi_downstream)
        multi_downstream
        ;;
    witness)
        witness
        ;;
    *)
        echo unknown run_test=$run_test
        exit 1
        ;;
esac
