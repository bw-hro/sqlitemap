#!/usr/bin/env bash

script_path=$(realpath "$0")
base_dir="$( dirname "$script_path" )"
build_dir="$base_dir/build"
toolchain_file="$base_dir/tools/vcpkg/scripts/buildsystems/vcpkg.cmake"

if [[ "${@#clean}" = "$@" ]]
then
    echo "use cache for project build"
else
    echo "clean project - remove $build_dir"
    rm -rf $build_dir
fi

if [[ "${@#release}" = "$@" ]]
then
    build_type="Debug"
    code_coverage="ON"
else
    build_type="Release"
    code_coverage="OFF"
    rm -rf $build_dir
fi
echo "project build type: $build_type"
echo "project code coverage: $code_coverage"

vcpkg_file="vcpkg.json"

if [[ "${@#without_tests}" = "$@" ]]
then
    build_tests="ON"
else
    build_tests="OFF"
fi
echo "project build tests: $build_tests"

if [[ "${@#skip_tests}" = "$@" ]]
then
    skip_tests="OFF"
else
    skip_tests="ON"
fi
echo "project skip tests: $skip_tests"

if [[ "${@#without_examples}" = "$@" ]]
then
    build_examples="ON"
else
    build_examples="OFF"
fi
echo "project build examples: $build_examples"

if [[ "${@#clang}" = "$@" ]]
then
    compiler="g++"
else
    compiler="clang++"
fi
echo "compiler: $compiler"

cmake -B build -S . \
    -D"SM_BUILD_TESTS=$build_tests" \
    -D"SM_SKIP_TESTS=$skip_tests" \
    -D"SM_BUILD_EXAMPLES=$build_examples" \
    -D"SM_ENABLE_COVERAGE=$code_coverage" \
    -D"CMAKE_BUILD_TYPE=$build_type" \
    -D"CMAKE_TOOLCHAIN_FILE=$toolchain_file" \
    -D"CMAKE_MAKE_PROGRAM:PATH=make" \
    -D"CMAKE_CXX_COMPILER=$compiler"

cmake --build build --parallel $(nproc)

ctest --test-dir build/test/