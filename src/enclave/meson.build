sgx_source_gen = generator(sgx_edger8r, output: ['@BASENAME@_t.c', '@BASENAME@_t.h'], arguments:['--trusted', '@INPUT@', '--search-path', sgx_sdk_dir+'/include/', '--search-path', enclave_dir, '--trusted-dir', '@BUILD_DIR@'])

sgx_tlibc_incdir = include_directories(sgx_sdk_dir+'/include/tlibc')
sgx_stdcxx_incdir = include_directories(sgx_sdk_dir+'/include/libcxx')

gen_trusted_enclave = sgx_source_gen.process(enclave_dir+'/Enclave.edl')

enclave_cpp_files = files(
    'Enclave.cpp',
    'Client.cpp',
    'Collection.cpp',
    'ias_ra.cpp',
    'Ledger.cpp',
    'Peer.cpp',
    'ObjectKeyProvider.cpp',
    'ObjectIterator.cpp',
    'ObjectListIterator.cpp',
    'Transaction.cpp',
    'TransactionExecutor.cpp',
    'TransactionManager.cpp',
    'TransactionProxy.cpp',
    'TransactionLedger.cpp',
    'TransactionHandle.cpp',
    'ObjectEventHandle.cpp',
    'RemoteParty.cpp',
    'PendingMessage.cpp',
    'bindings/Transaction.cpp',
    'bindings/NestedTransaction.cpp',
    'bindings/TransactionCollection.cpp',
    'bindings/Database.cpp',
    'bindings/OpContext.cpp',
    'bindings/ObjectIterator.cpp',
    'bindings/Object.cpp',
    'bindings/OpInfo.cpp',
    'bindings/Collection.cpp',
    'Shard.cpp',
    'op_info.cpp',
    'LockHandle.cpp',
    'RemoteParties.cpp',
    '../common/util/IdentityDatabase.cpp',
    '../common/util/Mutex.cpp',
    'Index.cpp',
    'MultiMap.cpp',
    'HashMap.cpp',
    '../common/util/MurmurHash2.cpp',
    'ProgramRunner.cpp',
    'TaskManager.cpp',
    'machineprimitives.cpp',
    'RemotePartyRunner.cpp',
    'Task.cpp',
    'BufferManager.cpp',
    'EncryptedIO.cpp',
    'LocalEncryptedIO.cpp',
    'RemoteEncryptedIO.cpp',
)

enclave_asm_files = files(
    'machineprimitives_asm.S'
)

libfakeenclave = static_library('fake_enclave', enclave_asm_files + enclave_cpp_files, c_args: compile_args + ['-DFAKE_ENCLAVE'], cpp_args: cpp_compile_args + ['-DFAKE_ENCLAVE'], include_directories: [credb_incdir, sgx_incdir])

sgx_enclave_signer = sgx_sdk_dir + '/bin/x64/sgx_sign'
