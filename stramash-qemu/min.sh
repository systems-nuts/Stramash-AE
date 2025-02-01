#!/bin/bash
./configure --target-list=aarch64-softmmu,x86_64-softmmu  --extra-cflags="-lrt -Wno-error=unused-function"   --disable-docs --disable-gtk --disable-opengl --disable-vnc --disable-virglrenderer --disable-werror 
