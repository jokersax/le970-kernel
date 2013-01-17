#!/bin/sh

export ARCH=arm
export CROSS_COMPILE=/mnt/android/cm9_kyle/prebuilt/linux-x86/toolchain/arm-eabi-4.4.3/bin/arm-eabi-

make j1a-perf_defconfig
make -j9
