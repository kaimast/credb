sgx_source_gen = generator(sgx_edger8r, output: ['@BASENAME@_t.c', '@BASENAME@_t.h'], arguments:['--trusted', '@INPUT@', '--search-path', sgx_sdk_dir+'/include/', '--search-path', enclave_dir, '--trusted-dir', '@BUILD_DIR@'])

sgx_tlibc_incdir = include_directories(sgx_sdk_dir+'/include/tlibc')
sgx_stdcxx_incdir = include_directories(sgx_sdk_dir+'/include/libcxx')

gen_trusted_enclave = sgx_source_gen.process(enclave_dir+'/Enclave.edl')

enclave_cpp_file_names=[
    'Enclave.cpp',
    'Clients.cpp',
    'Client.cpp',
    'Collection.cpp',
    'Peers.cpp',
    'ias_ra.cpp',
    'Ledger.cpp',
    'Peer.cpp',
    'Object.cpp',
    'Transaction.cpp',
    'ObjectEventHandle.cpp',
    'Block.cpp',
    'RemoteParty.cpp',
    'RWLockable.cpp',
    'PendingMessage.cpp',
    'bindings/Database.cpp',
    'bindings/OpContext.cpp',
    'bindings/ObjectIterator.cpp',
    'bindings/Object.cpp',
    'bindings/OpInfo.cpp',
    'bindings/Collection.cpp',
    'Shard.cpp',
    'LockHandle.cpp',
    'RemoteParties.cpp',
    '../common/util/IdentityDatabase.cpp',
    '../common/util/Mutex.cpp',
    'Index.cpp',
    'MultiMap.cpp',
    '../common/util/MurmurHash2.cpp',
    'ProgramRunner.cpp', 'TaskManager.cpp',
    'machineprimitives.cpp',
    'machineprimitives_asm.S',
    'RemotePartyRunner.cpp',
    'Task.cpp',
    'BufferManager.cpp',
    'StringIndex.cpp',
    'EncryptedIO.cpp',
    'RemoteEncryptedIO.cpp',
]

enclave_cpp_files = files(enclave_cpp_file_names)

sgx_enclave_signer = sgx_sdk_dir + '/bin/x64/sgx_sign'
