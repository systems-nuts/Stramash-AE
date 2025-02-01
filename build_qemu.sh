#!/bin/bash

CPU=16
pushd ./stramash-qemu/
sudo apt update 
pip install meson==1.3.2

./build.sh
cd build
export CFLAGS="-fdebug-prefix-map=$(pwd)=." 
export CXXFLAGS="-fdebug-prefix-map=$(pwd)=." 
export LDFLAGS="-Wl,--strip-debug"
make -j$16
popd
