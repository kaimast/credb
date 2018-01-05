ipp_inc=include_directories('../../external/ippcp_internal/inc')

ipp_lib_dir=meson.source_root()+'/external/ippcp_internal/lib/linux/intel64/'
ipp1_dep=cpp.find_library('ippcp', dirs: ipp_lib_dir)
ipp2_dep=cpp.find_library('ippcore', dirs:ipp_lib_dir)

ulibcrypto_cpp_files = files('init_tcrypto_lib.cpp', 'sgx_aes_ctr.cpp', 'sgx_aes_gcm.cpp', 'sgx_cmac128.cpp', 'sgx_ecc256.cpp', 'sgx_ecc256_common.cpp', 'sgx_ecc256_ecdsa.cpp', 'sgx_sha256.cpp', 'sgx_sha256_msg.cpp', 'tcrypto_version.cpp', 'memset_s.cpp', 'consttime_memequal.cpp')


