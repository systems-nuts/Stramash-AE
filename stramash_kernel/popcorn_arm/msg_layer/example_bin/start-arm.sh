#!/bin/bash
sudo ./qemu-system-aarch64 \
    -machine virt -cpu cortex-a53 -m 4096 -nographic \
    -drive id=root,if=none,media=disk,file=arm.img \
    -device virtio-blk-device,drive=root \
    -drive file=disk2.img,if=none,id=D1 \
    -device virtio-blk-device,drive=D1,serial=1234 \
    -kernel arm64-Image \
    -append "root=/dev/vdb console=ttyAMA0" \
