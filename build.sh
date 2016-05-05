#!/bin/bash

TOP=${PWD}
BUILD_DATE=$(date +"%Y%m%d.%H%M%S")
SRC_GIT_LOG=$(git log -1 --pretty=format:"%cd %an : %s" --date=short)
export BUILD_NUMBER=$(date +%Y%m%d)

#################################################################################
#             Environment Setting                                               #
#################################################################################
#set -x #debug echo on
TOOLCHAIN_ROOT=$HOME/gcc-linaro-4.9-2015.05-x86_64_arm-linux-gnueabihf
TOOLCHAIN_PREFIX=$TOOLCHAIN_ROOT/bin/arm-linux-gnueabihf-
thread_num=32

# echo color message
echoc()
{
    case $1 in
        red)    color=31;;
        green)  color=32;;
        yellow) color=33;;
        blue)   color=34;;
        purple) color=35;;
        *)      color=36;;
    esac
    echo -e "\033[;${color}m$2\033[0m"
}

export CROSS_COMPILE=$TOOLCHAIN_PREFIX
export ARCH=arm

[ -d build ] || mkdir out
make O=out distclean
echo '-KumaO' > .scmversion
make O=out panda_defconfig
make O=out  -j${thread_num} uImage || exit
#	make -j${thread_num} modules
#	make -j${thread_num} INSTALL_MOD_PATH=MOD_INSTALL modules_install
rm .scmversion
if [[ $? == "0" ]]; then
    echoc green "Build kernel, all success."
    echoc blue  out/arch/arm/boot/uImage
fi
