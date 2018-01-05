doxygen = find_program('doxygen', required : false)
if not doxygen.found()
  error(' doxygen not found.')
endif

subdir('cpp')
subdir('python')

cpp_html_target = custom_target('cpp',
                            input: cpp_doxyfile,
                            output: 'cpp',
                            command: [doxygen, cpp_doxyfile],
                            build_by_default: false)

python_html_target = custom_target('python',
                            input: py_doxyfile,
                            output: 'python',
                            command: [doxygen, py_doxyfile],
                            build_by_default: false)
