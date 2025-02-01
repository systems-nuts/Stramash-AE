#!/bin/bash
sudo umount temp_mnt1 &> /dev/null
sudo umount temp_mnt2 &> /dev/null

sudo mount x86_rootfs.img temp_mnt1
sudo mount arm_rootfs.img temp_mnt2

#sudo cp -p ./shm_version/popcorn_x86/msg_layer/msg_shm.ko temp_mnt1/root/shm_msg_layer.ko

sudo cp -p ./ipi_version/popcorn_x86/msg_layer/msg_shm.ko temp_mnt1/root/shm_msg_layer.ko

sudo umount temp_mnt1
sudo umount temp_mnt2



