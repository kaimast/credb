project('credb', 'c', 'cpp', default_options: ['buildtype=release'], version: '0.1.0')

sgx_sdk_dir = '/opt/intel/sgxsdk'
enclave_dir = '../enclave'

prefix_library_path=[get_option('prefix')+'/lib', get_option('prefix')+'/lib/x86_64-linux-gnu', '/usr/local/lib', '/usr/local/lib/x86_64-linux-gnu']

local_incdir = include_directories('./include', './src/common', get_option('prefix')+'/include', '/usr/local/include')
credb_incdir = include_directories('./include', './src/common', get_option('prefix')+'/include', '/usr/local/include')

sgx_library_path = sgx_sdk_dir + '/lib64'
sgx_incdir = include_directories(sgx_sdk_dir + '/include')
sgx_edger8r = find_program(sgx_sdk_dir + '/bin/x64/sgx_edger8r')

gmock = dependency('gmock')
gtest = dependency('gtest')

cpp = meson.get_compiler('cpp')

sgx_mode = get_option('sgx_mode')

if sgx_mode == 'simulation' or sgx_mode == 'fake_enclave'
    message('will link sgx_*_sim.so')
    sgx_urts_name = 'sgx_urts_sim'
    sgx_uae_name = 'sgx_uae_service_sim'
    sgx_trts_name = 'sgx_trts_sim'
    sgx_tservice_name = 'sgx_tservice_sim'
elif sgx_mode == 'prerelease' or sgx_mode == 'release'
    message('will link sgx_*.so')
    sgx_urts_name = 'sgx_urts'
    sgx_uae_name = 'sgx_uae_service'
    sgx_trts_name = 'sgx_trts'
    sgx_tservice_name = 'sgx_tservice'
else
    error('Invalid sgx mode: ' + sgx_mode)
endif

compile_args   = ['-Wextra', '-Werror']
cpp_compile_args = ['-std=c++17']

use_enclave = true
if sgx_mode == 'fake_enclave'
    compile_args = compile_args + ['-DFAKE_ENCLAVE']
    use_enclave = false
endif

if get_option('always_page') == true
    compile_args = compile_args + ['-DALWAYS_PAGE']
endif

if get_option('debug_mutex') == true
    compile_args = compile_args + ['-DDEBUG_MUTEX']
endif

if get_option('encrypt_files') == true
    compile_args = compile_args + ['-DENCRYPT_FILES']
endif

if get_option('buildtype') == 'release'
    if get_option('sgx_mode') == 'release' or get_option('sgx_mode') == 'simulation' or get_option('sgx_mode') == 'fake_enclave'
        compile_args += ['-DNDEBUG', '-UDEBUG']
    elif get_option('sgx_mode') == 'prerelease'
        compile_args += ['-DNDEBUG', '-DEDEBUG', '-DNSGX_DEBUG']
    else
        error('Unknown sgx_mode: ' + get_option('sgx_mode'))
    endif
elif get_option('buildtype') == 'debugoptimized'
    if get_option('sgx_mode') == 'release' or get_option('sgx_mode') == 'simulation' or get_option('sgx_mode') == 'fake_enclave'
        compile_args += ['-DDEBUG', '-UNDEBUG', '-UEDEBUG']
    elif get_option('sgx_mode') == 'prerelease'
        error('Cannot do prerelease and debugoptimized at the same time')
    else
        error('Unknown sgx_mode: ' + get_option('sgx_mode'))
    endif 
elif get_option('buildtype') == 'debug'
    if get_option('sgx_mode') == 'prerelease'
        error('Cannot do prerelease and debug at the same time')
    else 
        compile_args += ['-DDEBUG', '-UNDEBUG', '-UEDEBUG']
    endif
else
    error('Unknown buildtype: ' + get_option('buildtype'))
endif

py3_ext = import('python3')
py3_dep = dependency('python3')
boost_py3_dep = cpp.find_library('boost_python3')

if py3_ext.language_version() == '3.5'
    python_dir = 'lib/python3.5/dist-packages'
elif py3_ext.language_version() == '3.6'
    python_dir = 'lib/python3.6/dist-packages'
elif py3_ext.language_version() == '3.7'
    python_dir = 'lib/python3.7/dist-packages'
else
    error('Unknown python version')
endif

thread_dep = dependency('threads')
sgx_tcrypto_name = 'sgx_tcrypto'
sgx_uae_dep = cpp.find_library(sgx_uae_name, dirs: sgx_library_path)
sgx_tservice_dep = cpp.find_library(sgx_tservice_name, dirs: sgx_library_path)
sgx_trts_dep = cpp.find_library(sgx_trts_name, dirs: sgx_library_path)
sgx_tcrypto_dep = cpp.find_library(sgx_tcrypto_name, dirs:sgx_library_path)
sgx_tstdc_dep = cpp.find_library('sgx_tstdc', dirs: sgx_library_path)
sgx_tstdcxx_dep = cpp.find_library('sgx_tcxx', dirs: sgx_library_path)
log_dep = cpp.find_library('glog')
gflags_dep = cpp.find_library('gflags')
yael_dep = cpp.find_library('yael', dirs: prefix_library_path)
json_dep = cpp.find_library('document', dirs: prefix_library_path)
json_sgx_dep = cpp.find_library('document_sgx', dirs: prefix_library_path)
cowlang_dep = cpp.find_library('cowlang', dirs: prefix_library_path)
cowlang_sgx_dep = cpp.find_library('cowlang_sgx', dirs: prefix_library_path)
tkey_exchange_dep = cpp.find_library('sgx_tkey_exchange', dirs: sgx_library_path)
ukey_exchange_dep = cpp.find_library('sgx_ukey_exchange', dirs: sgx_library_path)
boost_po_dep = cpp.find_library('boost_program_options')
openssl_dep = cpp.find_library('crypto')

if get_option('sgx_mode') == 'release' or get_option('sgx_mode') == 'prerelease'
    # For release and prerelease we need to use the PSW's libraries
    sgx_urts_dep = cpp.find_library(sgx_urts_name)
else
    sgx_urts_dep = cpp.find_library(sgx_urts_name, dirs: sgx_library_path)
endif

subdir('src/ulibcrypto')
ulibcrypto = static_library('ucrypto',  ulibcrypto_cpp_files, dependencies: [openssl_dep], include_directories: [sgx_incdir, credb_incdir], c_args: compile_args, cpp_args: compile_args + cpp_compile_args)

subdir('src/enclave/')

if use_enclave
    common_args = ['-DIS_ENCLAVE', '-nostdinc', '-fvisibility=hidden', '-fstack-protector'] + compile_args
    enclave_so = executable('enclave.so', enclave_cpp_files, gen_trusted_enclave, 
    c_args: common_args, pie: true,
    cpp_args: common_args + cpp_compile_args + ['-nostdinc++'],
    link_args:['-Wl,--no-undefined','-nostdlib','-nodefaultlibs','-nostartfiles','-L'+sgx_sdk_dir+'/lib64',
    '-Wl,--whole-archive','-l'+sgx_trts_name,'-Wl,--no-whole-archive',
    '-Wl,--start-group', '-lsgx_tstdc','-lsgx_tcxx', '-lsgx_tkey_exchange','-l'+sgx_tcrypto_name,'-l'+sgx_tservice_name,'-ldocument_sgx','-lcowlang_sgx',
 '-Wl,--end-group',
    '-Wl,-Bstatic','-Wl,-Bsymbolic','-Wl,--no-undefined',
    '-Wl,-pie,-eenclave_entry','-Wl,--export-dynamic',
    '-Wl,--defsym,__ImageBase=0','-Wl,--gc-sections',
    '-Wl,--version-script='+meson.source_root()+'/src/enclave/Enclave.lds'],
    include_directories: [sgx_incdir, sgx_tlibc_incdir, sgx_stdcxx_incdir, local_incdir],
    dependencies: [json_sgx_dep, cowlang_sgx_dep])

    custom_target('enclave.signed.so', input: enclave_so, output:'enclave.signed.so', install:true, install_dir:'.',
        command:[sgx_enclave_signer, 'sign', '-key', meson.source_root()+'/src/enclave/Enclave_private.pem', '-enclave','enclave.so', '-out', 'enclave.signed.so', '-config',meson.source_root()+'/src/enclave/Enclave.config.xml']
    )
endif

subdir('src/server/')

server_incs = [credb_incdir, sgx_incdir]
server_deps = [yael_dep, log_dep, boost_po_dep, sgx_uae_dep, ukey_exchange_dep, sgx_urts_dep, thread_dep, gflags_dep]

if get_option('sgx_mode') == 'fake_enclave'
    server = executable('credb', server_cpp_files, include_directories: server_incs, dependencies: server_deps + [openssl_dep, cowlang_dep, json_dep], c_args: compile_args, cpp_args: compile_args + cpp_compile_args, install:true, link_with: [ulibcrypto, libenclave], link_args: ['-rdynamic'])
else
    server = executable('credb', server_cpp_files, gen_untrusted_enclave, include_directories: server_incs,
        dependencies: server_deps, c_args: compile_args, cpp_args: cpp_compile_args, install:true, link_with: ulibcrypto)
endif

subdir('src/client')

compile_args = compile_args + ['-DPYTHON_DATETIME=<python' + py3_ext.language_version()+'m/datetime.h>']

libcredb = shared_library('libcredb', client_cpp_files, c_args: compile_args, cpp_args: compile_args + cpp_compile_args, include_directories: [credb_incdir, sgx_incdir], install: true, dependencies: [openssl_dep, log_dep, yael_dep, json_dep, cowlang_dep, py3_dep], link_with: ulibcrypto, name_prefix: '', link_args: ['-lstdc++fs']) # Name hack so it doesn't conflict with python module

install_subdir('include/credb', install_dir : 'include')

witness_reader_files = ['src/ledger/witness_reader.cpp', 'src/common/util/base64.cpp']
witness_reader = executable('witness-reader', witness_reader_files, link_with: [libcredb], include_directories: [credb_incdir, sgx_incdir],
                            c_args: compile_args, cpp_args: compile_args + cpp_compile_args, dependencies: [json_dep])

client_incdir = include_directories('src/client')

py3_ext.extension_module('credb', python_file, dependencies: [py3_dep, openssl_dep, boost_py3_dep], install:true, include_directories: [sgx_incdir, credb_incdir, client_incdir], c_args: compile_args, cpp_args: compile_args + cpp_compile_args, install_dir: python_dir, link_with: [libcredb])

subdir('test/')

tests = executable('credb-test', test_cpp_files, test_extra_cpp_files, link_with: [libcredb, libfakeenclave], include_directories: [credb_incdir, sgx_incdir], c_args: test_compile_args, cpp_args: test_compile_args + cpp_compile_args, dependencies: [gtest, log_dep, gflags_dep, boost_py3_dep, py3_dep, yael_dep, json_dep, cowlang_dep],
                   link_args: ['-lstdc++fs'])
test('credb-test', tests)

subdir('doc')

# NOTE: gtest on ubuntu still uses deprecated functions so we can't lint the test files yet
clangtidy = find_program('clang-tidy', required: false)

if clangtidy.found()
    tidy_files = client_cpp_files + server_cpp_files + enclave_cpp_files

    run_target(
        'tidy',
        command: [
            'scripts/clang-tidy.sh',
            clangtidy.path(),
            meson.source_root(),
            meson.build_root(),
            tidy_files
        ],
    depends: [server, libcredb, libfakeenclave])
endif


