# Stramash AE

In popcorn\_kernel  and stramash kernel has the kernel source code. 
The kernel-config is the config 

mv kernel-config  .config
For x86 
make KCFLAGS="-fcf-protection=none" -j16

For arm
ARCH=arm64 CROSS\_COMPILE=aarch64-linux-gnu-   make -j16


Then please use the script in Stramash-QEMU


