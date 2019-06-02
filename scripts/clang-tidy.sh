#! /bin/bash
#! /bin/bash

CHECKS="hicpp-*,bugprone-*,modernize-*,misc-*,clang-analyzer-*"
DISABLED_CHECKS="-hicpp-signed-bitwise,-bugprone-exception-escape,-hicpp-member-init,-misc-non-private-member-variables-in-classes,-bugprone-macro-parentheses"

BIN=$1 && shift
PROJECT_ROOT=$1 && shift
MESON_ROOT=$1 && shift

# Execute in a different directory to ensure we don't mess with the meson config
TIDY_DIR=${PROJECT_ROOT}/build-tidy

mkdir -p ${TIDY_DIR}
cp  ${MESON_ROOT}/compile_commands.json ${TIDY_DIR}

# Replace meson commands clang does not understand
sed -i 's/-pipe//g' ${TIDY_DIR}/compile_commands.json

ALL_CHECKS="$CHECKS,$DISABLED_CHECKS"

echo "Running clang checks: ${ALL_CHECKS}"
$BIN -header-filter=${PROJECT_ROOT}/include/*.h,${PROJECT_ROOT}/src/*.h -checks=${ALL_CHECKS} -warnings-as-errors=* -p ${TIDY_DIR} $@
