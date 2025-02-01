#!/bin/bash
if [ "$1" = "ipi" ]; then
cd ./ipi_version/popcorn_x86/
make -j20
cd -
cd ./ipi_version/popcorn_arm/
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j20
cd -

elif [ "$1" = "qemu" ]; then
cd ../build
make -j20
cd -
cd ../build/contrib/plugins
make -j20
cd -

elif [ "$1" = "shm" ]; then
cd ./shm_version/popcorn_x86/
make -j20
cd -
cd ./shm_version/popcorn_arm/
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j20
cd -

else
echo "./build.sh [shm|ipi|qemu]"
fi
