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
elif [ $SYS == linux_i686 ]; then
  PLATFORM=Linux
  CXX="g++ -m32"
  STRIP=strip
elif [ $SYS == linux_armv5 ]; then
  PLATFORM=Linux
  CXX=arm-unknown-linux-gnueabi-g++
  STRIP=arm-unknown-linux-gnueabi-strip
elif [ $SYS == linux_aarch64 ]; then
  PLATFORM=Linux
  CXX=aarch64-unknown-linux-gnu-g++
  STRIP=aarch64-unknown-linux-gnu-strip
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
elif [ $SYS == freebsd_amd64 ]; then
  PLATFORM=Linux
  CXX="${FREEBSD_HOST}g++"
  FLAGS="-static-libgcc -static-libstdc++"
  STRIP="${FREEBSD_HOST}strip"
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
  CXX="$(xcrun -f g++) -arch x86_64"
  STRIP=$(xcrun -f strip)
elif [ $SYS == macos_i386 ]; then
  PLATFORM=Darwin
  export MACOSX_DEPLOYMENT_TARGET=10.8
  CXX="$(xcrun -f g++) -arch i386"
  STRIP=$(xcrun -f strip)
elif [ $SYS == ios_arm64 ]; then
  PLATFORM=Darwin
  export IPHONEOS_DEPLOYMENT_TARGET=7.0
  CXX="${IOS_HOST}clang++ -stdlib=libc++"
  CXX+=" -arch arm64"
  STRIP=${IOS_HOST}strip
  STATIC=""
elif [ $SYS == ios_arm64e ]; then
  PLATFORM=Darwin
  export IPHONEOS_DEPLOYMENT_TARGET=10.0
  CXX="${IOS_HOST}clang++ -stdlib=libc++"
  CXX+=" -arch arm64e"
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
  LIBS=$LIBS RELEASE=1 FLAGS="$FLAGS" \
  EXTRA_LIBS="$EXTRA_LIBS" -j

if [ -z "$DEBUG" ]; then
  $STRIP ${TOOLNAME}${EXE_SUFFIX}
fi

if [ $SYS == ios ]; then
  (which ldid &>/dev/null && ldid -S ${TOOLNAME}) || true
  chmod +x ${TOOLNAME}
fi
