datadir = join_paths(get_option('datadir'), 'doc', 'spede')

cdata = configuration_data()
cdata.set('VERSION', meson.project_version())
cdata.set('TOP_SRCDIR', meson.source_root())
cdata.set('TOP_BUILDDIR', meson.build_root())

py_doc_markdown = files('main.md')

py_doxyfile = configure_file(input: 'Doxyfile.in',
                          output: 'Doxyfile',
                          configuration: cdata,
                          install: false)

python_doc_file = custom_target('gen-doc-python-file',
                input: [],
                output: ['credb.py'],
                command: [meson.source_root() + '/doc/docparse.py', '@OUTPUT@'])
