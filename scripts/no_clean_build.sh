#!/bin/bash

# The script mainly sets up and builds CMake projects with Ninja
# TODO - point these to the correct binary locations on your system.

# Sets variables CMAKE and NINJA by finding their locations using the which command.
CMAKE=$(which cmake)
NINJA=$(which ninja)


# Creates a release build directory, configures the release build using CMake with Ninja as the generator.
mkdir -p ./cmake-build-release
$CMAKE -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM=$NINJA -G Ninja -S . -B ./cmake-build-release


# Builds the release target with 4 parallel jobs.
$CMAKE --build ./cmake-build-release --target all -j 4


# Creates a debug build directory, configures the debug build using CMake with Ninja as the generator.
mkdir -p ./cmake-build-debug
$CMAKE -DCMAKE_BUILD_TYPE=Debug -DCMAKE_MAKE_PROGRAM=$NINJA -G Ninja -S . -B ./cmake-build-debug

# Builds the debug target with 4 parallel jobs.

$CMAKE --build ./cmake-build-debug --target all -j 4