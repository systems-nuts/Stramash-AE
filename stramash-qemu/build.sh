#!/bin/bash
./configure --target-list=aarch64-softmmu,x86_64-softmmu  --extra-cflags="-lrt -Wno-error=unused-function"  --enable-debug --enable-plugins --disable-docs --disable-gtk --disable-opengl --disable-vnc --disable-virglrenderer --enable-debug-info --enable-trace-backends=log --disable-linux-aio --disable-linux-io-uring --disable-werror
#./configure --target-list=aarch64-softmmu,x86_64-softmmu --disable-werror --extra-cflags="-lrt" --enable-debug --enable-plugins
