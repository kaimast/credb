#! /bin/bash

INSTALL_DIR=$HOME/local
PY_VERSION=python3.5
SGX_DIR=${INSTALL_DIR}/intel
SGX_SDK_DIR=${SGX_DIR}/sgxsdk

TEST_PORT=52424

export LD_LIBRARY_PATH=$HOME/local/lib:$HOME/local/lib/x86_64-linux-gnu:/usr/local/lib:/usr/local/lib/x86_64-linux-gnu:${SGX_SDK_DIR}/lib64
export LIBRARY_PATH=$HOME/local/lib:$HOME/local/lib/x86_64-linux-gnu:/usr/local/lib:/usr/local/lib/x86_64-linux-gnu:${SGX_SDK_DIR}/lib64
export PYTHONPATH=${INSTALL_DIR}/lib/${PY_VERSION}/site-packages:${INSTALL_DIR}/lib/${PY_VERSION}/dist-packages
export PATH=${PATH}:${INSTALL_DIR}/bin

concurrent_transactions() {
    credb testserver --port $TEST_PORT &
    sleep 2
    test/concurrent_transactions.py --no_server
    ret=$?
    killall credb
    return $ret
}

multi_update() {
    credb testserver --port $TEST_PORT &
    sleep 2
    test/multi_update.py --num_updates=10000 --num_clients=20 --no_server
    ret=$?
    killall credb
    return $ret
}

multi_get() {
    credb testserver --port $TEST_PORT &
    sleep 2
    test/multi_get.py --num_gets=10000 --num_clients=20 --no_server
    ret=$?
    killall credb
    return $ret
}

multi_put() {
    credb testserver --port $TEST_PORT &
    sleep 2
    test/multi_put.py --num_puts=10000 --num_clients=20 --no_server
    ret=$?
    killall credb
    return $ret
}

call_program() {
    test/call_programs.py
    test/concurrent_remote_call_tx.py
    test/concurrent_call_programs.py
    test/concurrent_remote_call.py
}

bank_test() {
    test/bank_test.py
}

tpcc_docs() {
    credb testserver --port $TEST_PORT &
    sleep 10
    test/tpcc-docs.py
    ret=$?
    killall credb
    return $ret
}

debug_snapshot() {
    credb testserver > /dev/null 2>&1 &
    sleep 10
    test/debug_snapshot.py put --create_index
    test/debug_snapshot.py dump
    killall -9 credb

    credb testserver > /dev/null 2>&1 &
    sleep 10
    test/debug_snapshot.py load
    test/debug_snapshot.py find
    killall -9 credb
}

witness() {
    credb testserver > /dev/null 2>&1 &
    sleep 10
    test/witness.py --no_server --server_port 5042
    killall -9 credb
}

case $run_test in
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
    witness)
        witness
        ;;
    tpcc-docs)
        tpcc_docs
        ;;
    *)
        echo unknown run_test=$run_test
        exit 1
        ;;
esac
