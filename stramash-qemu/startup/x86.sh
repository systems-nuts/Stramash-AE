#!/bin/bash
TEST=2
TCP=
if [ "$3" = "tcp" ]; then
TCP=tcp
KERNEL=./kernel/x86/Image_TCP
MODE=3
elif [ "$3" = "shm" ]; then
KERNEL=./kernel/x86/Image_SHM
MODE=2
elif [ "$3" = "stramash" ]; then
KERNEL=./kernel/x86/Image_Stramash
MODE=1
elif [ "$3" = "stramash2" ]; then
KERNEL=./kernel/x86/Image_Stramash
MODE=1
TEST=3
elif [ "$3" = "op" ]; then
KERNEL=./kernel/x86/Image_Stramash
MODE=1
# TEST=1
else
echo "./x86.sh [use plugin] [id] [shm|tcp|stramash]"
exit 0
fi



FILE_SYSTEM=./x86_rootfs1.img
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
    sudo ../build/qemu-system-x86_64 \
	    -Stramashid $2 \
        -machine pc -m 8G -nographic \
	    -chardev socket,path=$SOCKET1,id=vintchar \
        -chardev socket,path=$SOCKET2,id=x86_chr \
        -drive id=root,if=none,readonly=$READONLY,media=disk,file=$FILE_SYSTEM \
        -device virtio-blk-pci,drive=root \
        -kernel $KERNEL \
        -append "nokaslr root=/dev/vda rw console=ttyS0" 
else
    sudo ../build/qemu-system-x86_64 \
        -Stramashid $2 \
        -machine pc -m 8G -nographic \
        -chardev socket,path=$SOCKET1,id=vintchar \
        -chardev socket,path=$SOCKET2,id=x86_chr \
        -drive id=root,if=none,readonly=$READONLY,media=disk,file=$FILE_SYSTEM \
        -device virtio-blk-pci,drive=root \
        -kernel $KERNEL \
        -append "nokaslr root=/dev/vda rw console=ttyS0" \
        -plugin $PLUGIN,test=$TEST,stramashid=$2,mode=$MODE \
        -d plugin \
        -icount shift=1
fi
./clean.sh $2
