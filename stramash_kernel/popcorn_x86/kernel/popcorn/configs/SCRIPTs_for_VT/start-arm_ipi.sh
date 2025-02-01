#!/bin/bash
sudo ./qemu-system-aarch64 \
    -machine virt -cpu cortex-a53 -m 8G -nographic \
    -chardev socket,path=/tmp/ivshmem_socket,id=vintchar \
    -chardev socket,path=/tmp/cross_ipi_chr,id=arm_chr \
    -drive id=root,if=none,readonly=off,media=disk,file=rootfs/arm_rootfs.img \
    -device virtio-blk-device,drive=root \
    -drive file=disk2.img,if=none,id=D1 \
    -device virtio-blk-device,drive=D1,serial=1234 \
    -icount shift=1 \
    -kernel arm64-Image \
    -append "root=/dev/vdb rw console=ttyAMA0" 
