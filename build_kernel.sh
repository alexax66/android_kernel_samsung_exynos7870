#!/bin/bash

export ARCH=arm64
export CROSS_COMPILE=~/build/toolchain/aarch64-cortex_a53-linux-gnueabi-7.4.0/bin/aarch64-cortex_a53-linux-gnu-
export ANDROID_MAJOR_VERSION=p

THREAD=-j$(bc <<< $(grep -c ^processor /proc/cpuinfo)+2)
DTBH_PLATFORM_CODE=0x50a6
DTBH_SUBTYPE_CODE=0x217584da


make O=./out $1
make O=./out $THREAD

$(pwd)/dtbTool  -o "$(pwd)/out/arch/arm64/boot/dtb.img" -s 2048 -d "$(pwd)/out/arch/arm64/boot/dts/" --platform $DTBH_PLATFORM_CODE --subtype $DTBH_SUBTYPE_CODE

cp out/arch/arm64/boot/Image  out/arch/arm64/boot/recovery.img-kernel
cp out/arch/arm64/boot/dtb.img  out/arch/arm64/boot/recovery.img-dt
