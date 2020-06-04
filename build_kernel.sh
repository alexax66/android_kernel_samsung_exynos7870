#!/bin/bash

export ARCH=arm64
export CROSS_COMPILE=~/build/toolchain/gcc-linaro-7.5.0-linux-gnu/bin/aarch64-linux-gnu-
export ANDROID_MAJOR_VERSION=q

THREAD=-j$(bc <<< $(grep -c ^processor /proc/cpuinfo)+2)

make O=./out $1
make O=./out $THREAD

cp out/arch/arm64/boot/Image  out/arch/arm64/boot/boot.img-kernel
cp out/arch/arm64/boot/dtb.img  out/arch/arm64/boot/boot.img-dt