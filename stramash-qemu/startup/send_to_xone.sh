#!/bin/bash
SERVER=cong@mario
scp ./shm_version/popcorn_arm/arch/arm64/boot/Image $SERVER:/home/cong/qemu-stramash/startup/shm_version/popcorn_arm/arch/arm64/boot/Image
scp ./shm_version/popcorn_x86/arch/x86/boot/bzImage $SERVER:/home/cong/qemu-stramash/startup/shm_version/popcorn_x86/arch/x86/boot/bzImage
scp ./shm_version/popcorn_x86/msg_layer/msg_shm.ko $SERVER:/home/cong/qemu-stramash/startup/shm_version/popcorn_x86/msg_layer/msg_shm.ko
scp ./shm_version/popcorn_arm/msg_layer/msg_shm.ko $SERVER:/home/cong/qemu-stramash/startup/shm_version/popcorn_arm/msg_layer/msg_shm.ko
