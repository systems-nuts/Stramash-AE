#!/bin/bash
sudo ./qemu-system-x86_64 \
    -machine pc -m 4096 -nographic \
    -drive id=root,if=none,media=disk,file=x86.img \
    -device virtio-blk-pci,drive=root \
    -drive file=disk1.img,if=none,id=D1 \
    -device virtio-blk-pci,drive=D1,serial=1234 \
    -kernel x86_64-Image \
    -append "root=/dev/vda1 console=ttyS0" \
