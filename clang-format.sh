#! /bin/bash

# Will run clang-format to apply formatting guidlines from .clang-format

find src -iname *.h -o -iname *.cpp | xargs clang-format -i
find ./include -iname *.h -o -iname *.cpp | xargs clang-format -i
