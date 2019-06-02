sgx_source_gen = generator(sgx_edger8r, output: ['@BASENAME@_u.c', '@BASENAME@_u.h'], arguments:['--untrusted', '@INPUT@', '--search-path', sgx_sdk_dir+'/include/', '--search-path', enclave_dir, '--untrusted-dir', '@BUILD_DIR@'])

gen_untrusted_enclave = sgx_source_gen.process(enclave_dir+'/Enclave.edl')

server_cpp_files=files(
    'RemoteParty.cpp',
    'RemoteParties.cpp',
    'FakeEnclave.cpp',
    'Disk.cpp',
    'main.cpp',
    'Server.cpp',
    'ClientAcceptor.cpp',
    'ClientHandler.cpp',
    'Attestation.cpp',
    'EnclaveHandle.cpp',
    'PeerAcceptor.cpp',
    'PeerHandler.cpp',
    '../common/util/base64.cpp'
)
