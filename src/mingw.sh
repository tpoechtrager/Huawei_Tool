#!/bin/sh

set -x
make clean
make CXX=i686-w64-mingw32-g++ LTO=0 PLATFORM=mingw LIBS=../libs/win32 -j8
