#!/bin/bash
stramash=./stramash_kernel
popcorn=popcorn_kernel
target=stramash-qemu/startup

mkdir -p $target/kernel/arm
mkdir -p $target/kernel/x86
cp -r $stramash/popcorn_arm/arch/arm64/boot/Image $target/kernel/arm/Image_Stramash
cp -r $stramash/popcorn_x86/arch/x86/boot/bzImage $target/kernel/x86/Image_Stramash
cp -r $stramash/popcorn_x86/msg_layer/msg_shm.ko $target/kernel/x86/stramash_msg_shm.ko
cp -r $stramash/popcorn_arm/msg_layer/msg_shm.ko $target/kernel/arm/stramash_msg_shm.ko

cp -r $popcorn/popcorn_arm/arch/arm64/boot/Image $target/kernel/arm/Image_SHM
cp -r $popcorn/popcorn_x86/arch/x86/boot/bzImage $target/kernel/x86/Image_SHM
cp -r $popcorn/popcorn_x86/msg_layer/msg_shm.ko $target/kernel/x86/shm_msg_shm.ko
cp -r $popcorn/popcorn_arm/msg_layer/msg_shm.ko $target/kernel/arm/shm_msg_shm.ko

