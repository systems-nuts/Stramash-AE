#!/bin/bash
TEST=2
TCP=
if [ "$3" = "tcp" ]; then
TCP=tcp
KERNEL=./kernel/arm/Image_TCP
MODE=3
elif [ "$3" = "shm" ]; then
KERNEL=./kernel/arm/Image_SHM
MODE=2
elif [ "$3" = "stramash" ]; then
KERNEL=./kernel/arm/Image_Stramash
MODE=1
elif [ "$3" = "stramash2" ]; then
KERNEL=./kernel/arm/Image_Stramash
MODE=1
TEST=3
elif [ "$3" = "op" ]; then
KERNEL=./kernel/arm/Image_Stramash
MODE=1
# TEST=1
else
echo "./arm.sh [skip plugin] [id] [shm|tcp|stramash]"
exit 0
fi



FILE_SYSTEM=./arm_rootfs1.img 
SOCKET1=/tmp/ivshmem_socket$2
SOCKET2=/tmp/cross_ipi_chr$2
READONLY=on
PLUGIN=../build/contrib/plugins/libcache-sim.so
if [ "$4" = "1" ]; then
    PLUGIN=../build/contrib/plugins/libcache-sim-break.so
elif [ "$4" = "2" ]; then
    PLUGIN=../build/contrib/plugins/libcache-sim-feedback.so
fi

if [ "$1" = "1" ]; then
    sudo ../build/qemu-system-aarch64 \
	    -Stramashid $2 \
        -machine virt -cpu cortex-a76 -m 8G -nographic \
        -smp 1 \
        -chardev socket,path=$SOCKET1,id=vintchar \
        -chardev socket,path=$SOCKET2,id=arm_chr \
        -drive id=root,if=none,readonly=$READONLY,media=disk,file=$FILE_SYSTEM\
        -device virtio-blk-device,drive=root \
        -drive file=disk2.img,if=none,readonly=on,id=D1 \
        -device virtio-blk-device,drive=D1,serial=2234 \
        -kernel $KERNEL \
        -append "nokaslr root=/dev/vdb rw console=ttyAMA0" 

else
    sudo ../build/qemu-system-aarch64 \
        -machine virt -cpu cortex-a76 -m 8G -nographic \
        -smp 1 \
        -Stramashid $2 \
        -chardev socket,path=$SOCKET1,id=vintchar \
        -chardev socket,path=$SOCKET2,id=arm_chr \
        -drive id=root,if=none,readonly=$READONLY,media=disk,file=$FILE_SYSTEM \
        -device virtio-blk-device,drive=root \
        -drive file=disk2.img,if=none,readonly=on,id=D1 \
        -device virtio-blk-device,drive=D1,serial=2234 \
        -kernel $KERNEL \
        -append "nokaslr root=/dev/vdb rw console=ttyAMA0" \
        -plugin $PLUGIN,test=$TEST,stramashid=$2,mode=$MODE \
        -d plugin \
        -icount shift=1
fi
# ,sleep=off,align=off
./clean.sh $2
