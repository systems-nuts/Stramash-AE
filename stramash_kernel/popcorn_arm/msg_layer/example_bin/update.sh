#!/bin/bash
sudo cp popcorn_x86/arch/x86/boot/bzImage x86_64-Image
sudo cp popcorn_arm/arch/arm64/boot/Image arm64-Image
sudo mount disk2.img mount_2
sudo mount disk1.img mount_1
sudo cp popcorn_x86/msg_layer/msg_shm.ko mount_1/
sudo cp popcorn_x86/msg_layer/msg_socket.ko mount_1/
sudo cp test* mount_1/
sudo cp test* mount_2/
sudo cp is* mount_1/
sudo cp is* mount_2/
sudo cp popcorn_arm/msg_layer/msg_socket.ko mount_2/
sudo cp popcorn_arm/msg_layer/msg_shm.ko mount_2/
sudo umount mount_1
sudo umount mount_2
