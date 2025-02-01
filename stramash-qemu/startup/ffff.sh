#!/bin/bash
FILE_SYSTEM=./arm_rootfs$2.img 
KERNEL=./shm_version/popcorn_arm/arch/arm64/boot/Image
#KERNEL=/home/tong/SMARTNIC/smartnic_arm/arch/arm64/boot/Image
SOCKET1=/tmp/ivshmem_socket$2
SOCKET2=/tmp/cross_ipi_chr$2

if [ "$1" = "1" ]; then
    sudo ../build/qemu-system-aarch64 \
        -machine virt -cpu cortex-a76 -m 8G -nographic \
        -chardev pty,id=char0 -serial chardev:char0 \
        -monitor stdio \
        -smp 1 \
        -chardev socket,path=$SOCKET1,id=vintchar \
        -chardev socket,path=$SOCKET2,id=arm_chr \
        -drive id=root,if=none,readonly=off,media=disk,file=$FILE_SYSTEM\
        -device virtio-blk-device,drive=root \
        -drive file=disk2.img,if=none,readonly=on,id=D1 \
        -device virtio-blk-device,drive=D1,serial=2234 \
        -kernel $KERNEL \
        -append "root=/dev/vdb rw console=ttyAMA0" 

else
    sudo ../build/qemu-system-aarch64$2 \
        -machine virt -cpu cortex-a53 -m 8G -nographic \
        -chardev socket,path=$SOCKET1,id=vintchar \
        -chardev socket,path=$SOCKET2,id=arm_chr \
        -drive id=root,if=none,readonly=off,media=disk,file=$FILE_SYSTEM \
        -device virtio-blk-device,drive=root \
        -drive file=disk2.img,if=none,readonly=on,id=D1 \
        -device virtio-blk-device,drive=D1,serial=2234 \
        -kernel $KERNEL \
        -append "root=/dev/vdb rw console=ttyAMA0" \
        -plugin ../build/contrib/plugins/libcache-sim.so \
        -d plugin \
        -icount shift=1
fi
