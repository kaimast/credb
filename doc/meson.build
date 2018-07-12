doxygen = find_program('doxygen', required : false)

if doxygen.found()
    subdir('cpp')
    subdir('python')

    doc_theme_files = files('theme/style.css')

    cpp_html_target = custom_target('cpp',
                                input: [cpp_doxyfile, cpp_doc_markdown, doc_theme_files],
                                output: 'cpp',
                                command: [doxygen, cpp_doxyfile],
                                build_by_default: false)

    python_html_target = custom_target('python',
                                input: [py_doxyfile, py_doc_markdown, doc_theme_files, python_doc_file],
                                output: 'python',
                                command: [doxygen, py_doxyfile],
                                build_by_default: false)
endif
