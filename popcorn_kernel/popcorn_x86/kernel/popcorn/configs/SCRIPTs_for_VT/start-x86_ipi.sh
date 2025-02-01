#!/bin/bash
sudo ./qemu-system-x86_64 \
    -machine pc -m 8G -nographic \
    -chardev socket,path=/tmp/ivshmem_socket,id=vintchar \
    -chardev socket,path=/tmp/ivshmem_socket,id=ivsh \
    -chardev socket,path=/tmp/cross_ipi_chr,id=x86_chr \
    -device ivshmem-doorbell,vectors=1,chardev=ivsh \
    -drive id=root,if=none,readonly=off,media=disk,file=rootfs/x86_rootfs.img \
    -device virtio-blk-pci,drive=root \
    -drive file=disk1.img,if=none,id=D1 \
    -device virtio-blk-pci,drive=D1,serial=1234 \
    -netdev tap,id=mynet0,ifname=tap0,script=no,downscript=no \
    -net nic,model=e1000,netdev=mynet0 \
    -icount shift=1 \
    -kernel x86_64-Image \
    -append "root=/dev/vda rw console=ttyS0" 
