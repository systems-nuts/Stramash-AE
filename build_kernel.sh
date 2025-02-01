#!/bin/bash
set -e

CPU=16

cd ./popcorn_kernel/popcorn_x86
cp kernel-config .config
make oldconfig
make KCFLAGS="-fcf-protection=none" -j$CPU
cd -

cd ./popcorn_kernel/popcorn_arm
cp kernel-config .config
ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- make oldconfig
ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- make -j$CPU
cd -

cd ./stramash_kernel/popcorn_x86
cp kernel-config .config
make oldconfig
make KCFLAGS="-fcf-protection=none" -j$CPU
cd -

cd ./stramash_kernel/popcorn_arm
cp kernel-config .config
ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- make oldconfig
ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- make -j$CPU
cd -
