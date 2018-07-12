#! /usr/bin/python3

from inspect import *
import credb
import sys

out = open(sys.argv[1], "w")

def spaces(count):
    res = ""
    for _ in range(count):
        res += " "
    return res

def printdoc(obj, indent):
    for name, member in getmembers(obj):
        if name[0:2] == "__":
            continue

        if name == "IsolationLevel":
            continue #FIXME

        params = []

        if member.__doc__:
            for line in member.__doc__.split('\n'):
                # String for doxygen
                if line.isspace():
                    continue

                if line.startswith(" @param"):
                    param = line.replace(" @param ", "").replace(" [optional]", "")
                    params.append(param)
                
                out.write(spaces(indent) + "##" + line + "\n")

        if isclass(member):
            out.write(spaces(indent) + "class " + name + ":\n")
            printdoc(member, indent+4)
            out.write("\n")

        elif ismethod(member) or isfunction(member) or isroutine(member):
        #    out.write(spaces(indent) + "def " + name + signature(member) + ":")
            out.write(spaces(indent) + "def " + name + "(" + ','.join(params) + "):\n")
            out.write(spaces(indent) + "    pass\n")

        else:
            continue

out.write("# NOTE: this is generated code only intended for doxygen \n")
printdoc(credb, 0)
