#!/bin/sh
# Builds and runs the unit tests with the host's C++ compiler (no SDK or libntr needed)
set -e
cd "$(dirname "$0")"
${CXX:-g++} -std=c++17 -Wall -Wextra -Werror -I../../src/port gen5_rom_fs_test.cpp -o gen5_rom_fs_test.exe
./gen5_rom_fs_test.exe
