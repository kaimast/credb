#! /usr/bin/python3

import re
import sys

def parse_doc_string(istr):
    pattern = re.compile(r'@(\w+)\s+(.*)')
    docstring = list()
    for line in map(lambda s : s.strip(), istr):
        if line[0:2] == '/**':
            continue
        if line == '*/':
            return docstring
        line = line.lstrip('* ')
        line = line.replace('"', '\\"')
        docstring.append(line)

        #match = pattern.match(line)
        #if match:
         #   docstring.append((match.group(1), match.group(2)))

def extract(istr, docstrings):
    pattern = re.compile(r'^\s*\*\s*@label\{(\w+)\}$')
    for line in map(lambda s : s.strip(), istr):
        match = pattern.match(line)
        if match:
            token = match.group(1)
            docstrings[token] = parse_doc_string(istr)

def format_doc_string(docstring):
    return '\n'.join(docstring)
#    return '\n'.join('{}: {}'.format(k, v) for (k, v) in docstring)

def escape(string):
    return string.replace('\n', r'\n')

def substitute(istr, ostr, docstrings):
    pattern = re.compile(r'@DocString\((\w+)\)')
    for line in map(lambda s : s.rstrip(), istr):
        for match in pattern.finditer(line):
            token = match.group(1)
            docstring = format_doc_string(docstrings[token])
            line = line.replace(match.group(0), escape(docstring))
        print(line, file=ostr)

if __name__ == '__main__':
    cpp_files = ['Witness.h', 'Client.h', 'Transaction.h', 'Collection.h']
    sourcefile = "src/client/python_api.cpp.in"

    path = sys.argv[1]
    outfile = sys.argv[2]
    docstrings = dict()

    for cpp_file in cpp_files:
        with open(path + "/include/credb/" + cpp_file, "r") as istr:
            extract(istr, docstrings)

    with open(path + "/" + sourcefile, "r") as istr:
        with open(outfile, "w") as ostr:
            ostr.write('/// GENERATED FROM python_api.cpp.in BY A SCRIPT. DO NOT MODIFY DIRECTLY. ///\n')
            substitute(istr, ostr, docstrings)
