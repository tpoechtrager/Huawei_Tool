#!/usr/bin/env bash

set -e
pushd "${0%/*}" &>/dev/null

TOOLNAME="huawei_band_tool"

[ -z "$SYS" ] && SYS=linux_x86_64

LTO_JOBS=8
STD=c++14
STATIC="-static-libgcc -static-libstdc++"

if [ $SYS == linux_x86_64 ]; then
  CXX=g++
  STRIP=strip
  EXTRA_FLAGS="-flto=${LTO_JOBS} -Wl,--gc-sections -lrt"
  EXE_SUFFIX=""
elif [ $SYS == linux_i686 ]; then
  CXX="g++ -m32"
  STRIP=strip
  EXTRA_FLAGS="-flto=${LTO_JOBS} -Wl,--gc-sections -lrt"
  EXE_SUFFIX=""
elif [ $SYS == openwrt_mips_uclibc ]; then
  openwrt_uclibc
  CXX=mips-openwrt-linux-g++
  STRIP=mips-openwrt-linux-strip
  STD=c++11
  EXTRA_FLAGS="-Os -flto=${LTO_JOBS} -lgcc_eh -Wl,--gc-sections"
  EXE_SUFFIX=""
elif [ $SYS == openwrt_mips_musl ]; then
  openwrt_musl
  CXX=mips-openwrt-linux-g++
  STRIP=mips-openwrt-linux-strip
  STATIC+=" -static"
  EXTRA_FLAGS="-Os -flto=${LTO_JOBS} -lgcc_eh -Wl,--gc-sections"
  EXE_SUFFIX=""
elif [ $SYS == win32 ]; then
  CXX=i686-w64-mingw32-g++
  STRIP=i686-w64-mingw32-strip
  EXTRA_FLAGS="-lws2_32 -lwldap32 -flto=${LTO_JOBS} -Wl,--gc-sections"
  EXE_SUFFIX=".exe"
elif [ $SYS == macos_x86_64 ]; then
  export MACOSX_DEPLOYMENT_TARGET=10.8
  CXX="$(xcrun -f clang++) -stdlib=libc++"
  STRIP=$(xcrun -f strip)
  EXTRA_FLAGS="-DUSE_GETTIMEOFDAY -Wl,-dead_strip"
  EXE_SUFFIX=""
  STATIC=""
elif [ $SYS == ios ]; then
  export IPHONEOS_DEPLOYMENT_TARGET=7.1
  CXX="${IOS_HOST}clang++ -stdlib=libc++"
  CXX+=" -arch armv7 -arch armv7s -arch arm64"
  STRIP=${IOS_HOST}strip
  EXTRA_FLAGS="-DUSE_GETTIMEOFDAY -Wl,-dead_strip"
  EXE_SUFFIX=""
  STATIC=""
else
  exit 1
fi

set -x

$CXX \
  -std=$STD -Wall -Wextra \
  $(ls *.cpp | grep -v tty_proxy) \
  -I . \
  -DCURL_STATICLIB -O3  \
  -isystem ../libs/$SYS/include \
  -L ../libs/$SYS/lib \
  -lcryptopp -lcurl -lconfig4cpp \
  $STATIC \
  $EXTRA_FLAGS \
  -o ${TOOLNAME}${EXE_SUFFIX}

$STRIP ${TOOLNAME}${EXE_SUFFIX}

if [ $SYS == ios ]; then
  (which ldid &>/dev/null && ldid -S ${TOOLNAME}) || true
  chmod +x ${TOOLNAME}
fi
