test_cpp_files = files(
    'Ledger.cpp',
    'TransactionLedger.cpp',
    'Programs.cpp',
    'primitives.cpp',
    'Workload.cpp',
    'basic.cpp',
    'main.cpp',
    'Order.cpp',
    'Transaction.cpp',
    'IsolationLevels.cpp',
    'BufferManager.cpp',
    'HashMap.cpp',
    'MultiMap.cpp',
    'Disk.cpp',
    'LockHandle.cpp',
    'RemoteTransaction.cpp',
    'TransactionManager.cpp'
)

#FIXME provide cleaner abstractions so not all this stuff gets pulled in
test_extra_cpp_files = files(
    '../src/server/Disk.cpp',
    '../src/server/FakeEnclave.cpp',
    '../src/server/RemoteParties.cpp',
    '../src/server/RemoteParty.cpp',
    '../src/server/EnclaveHandle.cpp',
    '../src/server/Attestation.cpp',
    '../src/ledger/Witness.cpp',
)

test_compile_args = compile_args + ['-DIS_TEST', '-DFAKE_ENCLAVE']
