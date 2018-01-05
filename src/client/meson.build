client_cpp_files = files('TransactionImpl.cpp', 'ClientImpl.cpp', 'CollectionImpl.cpp', 'TransactionCollectionImpl.cpp', 'PendingMessage.cpp', 'ecp.cpp', '../ledger/Witness.cpp', 'DocParser.cpp', '../common/util/base64.cpp')

python_file = custom_target('gen-python-file',
                input: ['python_api.cpp.in'],
                output: ['python_api.cpp'],
                command: [meson.source_root() + '/generate_python_doc.py', meson.source_root(), '@OUTPUT@'])
