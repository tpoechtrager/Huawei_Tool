#!/usr/bin/env bash

set -e
pushd "${0%/*}" &>/dev/null

TOOLNAME="huawei_band_tool"

[ -z "$SYS" ] && SYS=linux_x86_64

LTO_JOBS=8
STD=c++14
LIBS=../libs/$SYS

if [ $SYS == linux_x86_64 ]; then
  PLATFORM=Linux
  CXX=g++
  STRIP=strip
  EXTRA_FLAGS="-flto=${LTO_JOBS} -Wl,--gc-sections -lrt"
elif [ $SYS == linux_i686 ]; then
  PLATFORM=Linux
  CXX="g++ -m32"
  STRIP=strip
  EXTRA_FLAGS="-flto=${LTO_JOBS} -Wl,--gc-sections -lrt"
elif [ $SYS == linux_armv5 ]; then
  PLATFORM=Linux
  CXX=arm-unknown-linux-gnueabi-g++
  STRIP=arm-unknown-linux-gnueabi-strip
  EXTRA_FLAGS="-flto=${LTO_JOBS} -Wl,--gc-sections -lrt"
elif [ $SYS == openwrt_mips_uclibc ]; then
  PLATFORM=Linux
  openwrt_uclibc
  CXX=mips-openwrt-linux-g++
  STRIP=mips-openwrt-linux-strip
  EXTRA_LIBS+=" -lgcc_eh"
  STD=c++11
elif [ $SYS == openwrt_mips_musl ]; then
  PLATFORM=Linux
  openwrt_musl
  CXX=mips-openwrt-linux-g++
  STRIP=mips-openwrt-linux-strip
  EXTRA_LIBS+=" -lgcc_eh -static"
elif [ $SYS == win32 ]; then
  PLATFORM=MinGW
  CXX=i686-w64-mingw32-g++
  STRIP=i686-w64-mingw32-strip
  EXE_SUFFIX=".exe"
elif [ $SYS == win64 ]; then
  PLATFORM=MinGW
  CXX=x86_64-w64-mingw32-g++
  STRIP=x86_64-w64-mingw32-strip
  EXE_SUFFIX=".exe"
elif [ $SYS == macos_x86_64 ]; then
  PLATFORM=Darwin
  export MACOSX_DEPLOYMENT_TARGET=10.8
  CXX="$(xcrun -f clang++) -stdlib=libc++"
  STRIP=$(xcrun -f strip)
elif [ $SYS == ios ]; then
  PLATFORM=Darwin
  export IPHONEOS_DEPLOYMENT_TARGET=7.1
  CXX="${IOS_HOST}clang++ -stdlib=libc++"
  CXX+=" -arch armv7 -arch armv7s -arch arm64"
  STRIP=${IOS_HOST}strip
  STATIC=""
elif [ $SYS == android_armv7 ]; then
  PLATFORM=Android
  CXX="arm-linux-androideabi-clang++"
  STRIP="arm-linux-androideabi-strip"
  STATIC=""
else
  exit 1
fi

set -x

make clean

make \
  CXX="$CXX" STD=$STD PLATFORM=$PLATFORM \
  LIBS=$LIBS RELEASE=1 EXTRA_LIBS="$EXTRA_LIBS" -j8

$STRIP ${TOOLNAME}${EXE_SUFFIX}

if [ $SYS == ios ]; then
  (which ldid &>/dev/null && ldid -S ${TOOLNAME}) || true
  chmod +x ${TOOLNAME}
fi
