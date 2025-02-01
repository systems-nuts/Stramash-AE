#!/bin/bash
TEST=1
stramashid=1
sudo ~/qemu/build/qemu-x86_64 -plugin ~/qemu/build/contrib/plugins/libcache-sim-user.so,us,test=$TEST,stramashid=$2,mode=1 -d plugin $1