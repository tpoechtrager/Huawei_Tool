#!/usr/bin/env bash

set -e
pushd "${0%/*}" &>/dev/null

ln -sf ~/build/libs ..

TOOLNAME="huawei_band_tool"
RELEASEDIR="../release"

function trim_line()
{
  IFS=' '
  line=""
  for w in $1; do
    [ $w == \\ ] && continue
    if [ -n "$line" ]; then
      line+=" $w"
    else
      line="$w"
    fi
  done

  echo $line
}

## Awful but still better than doing this by hand :-) ##

function convert_shell_to_batch()
{
  pushd .. &>/dev/null
  files=$(ls *.sh)

  for file in $files; do
    echo "converting $file to batch..."
    in=$(cat $file)
    in=$(echo $in   | sed 's/#!\/bin\/sh//')
    out=$(echo $in  | sed 's/\.\///g')
    out=$(echo $out | sed 's/\.sh/.bat/g')
    out=$(echo $out | sed 's/sleep/PING 192.0.2.2 -n 1 -w/g')
    out=$(echo $out | sed 's/echo ""/echo[/g')
    out=$(echo $out | sed "s/ && /\\n/g")
    code_out=""
    code_out="${code_out}@echo off"$'\n'
    code_out="${code_out}REM Automatically generated"$'\n'
    code_out="${code_out}REM Input:${in}"$'\n'
    code_out="${code_out}@echo on"$'\n'$'\n'
    IFS=$'\n'
    for line in $out; do
      line=$(trim_line $line)
      if [[ $line != ${TOOLNAME}\ * ]]; then
        code_out="${code_out}@echo off"$'\n'
      fi
      code_out="${code_out}${line}"
      if [[ $line == PING\ 192.0.2.2\ -n\ 1\ -w\ * ]]; then
        code_out="${code_out}000 >NUL"
      fi
      code_out="${code_out}"$'\n'
      [[ $line == ${TOOLNAME}\ * ]] && code_out="${code_out}@echo off"$'\n'
      code_out="${code_out}if %errorlevel% neq 0 (EXIT %errorlevel%)"$'\n'
      code_out="${code_out}@echo on"$'\n'$'\n'
    done
    code_out=${code_out::-1}
    bat=src/${RELEASEDIR}/$1/$(echo $file | sed 's/\.sh/.bat/g')
    echo "$code_out" > $bat
    unix2dos $bat
  done

  popd &>/dev/null
}

function build()
{
  rm -f ${TOOLNAME}{,.exe}
  SYS=$1 ./compile.sh
  mkdir -p ${RELEASEDIR}/$2
  if [ -f ${TOOLNAME}.exe ]; then
    cp ${TOOLNAME}.exe ${RELEASEDIR}/$2
  else
    cp ${TOOLNAME} ${RELEASEDIR}/$2
    cp ../*.sh ${RELEASEDIR}/$2
    if [[ $1 == android* ]]; then
      sed -i'' 's/#!\/bin\/sh//' ${RELEASEDIR}/$2/*.sh
    fi
  fi
  cp ../${TOOLNAME}_config.txt ${RELEASEDIR}/$2
  if [ -f ${TOOLNAME}.exe ]; then
    unix2dos ${RELEASEDIR}/$2/${TOOLNAME}_config.txt
    convert_shell_to_batch $2
  else
    dos2unix ${RELEASEDIR}/$2/${TOOLNAME}_config.txt
  fi
  if [[ $1 == *openwrt* ]]; then
    upx -9 ${RELEASEDIR}/$2/${TOOLNAME}
  fi
}

rm -rf $(ls -d ${RELEASEDIR}/*/)
rm -rf ${RELEASEDIR}/{*.zip,*.bz2}

build linux_armv5 Linux/armv5
build android_armv7 Android/armv7
build linux_x86_64 Linux/64bit
build linux_i686 Linux/32bit
build openwrt_mips_musl OpenWRT/MIPS/32bit/big_endian/musl_static
build openwrt_mips_uclibc OpenWRT/MIPS/32bit/big_endian/uclibc
build macos_x86_64 MacOS/64bit
build ios iOS/armv7_armv7s_arm64
build win32 Windows/32bit
build win64 Windows/64bit

unix2dos ${RELEASEDIR}/*.txt
version=$(cat version.h | grep "define VERSION" | xargs | awk '{print $3}')

pushd ${RELEASEDIR} &>/dev/null
zip -q -9 -r - * > ../${TOOLNAME}_v${version}.zip
tar cf - * | bzip2 -9 - > ../${TOOLNAME}_v${version}.tar.bz2
mv ../${TOOLNAME}_v${version}{.zip,.tar.bz2} ${RELEASEDIR}
echo ""
echo "SHA256:"
sha256sum ${TOOLNAME}_v${version}{.zip,.tar.bz2}
echo ""
echo "SHA1:"
sha1sum ${TOOLNAME}_v${version}{.zip,.tar.bz2}
echo ""
echo "MD5:"
md5sum ${TOOLNAME}_v${version}{.zip,.tar.bz2}
popd &>/dev/null

make clean 1>/dev/null
