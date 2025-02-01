#!/bin/bash
sudo rm /dev/shm/*
sudo rm /tmp/cross*
sudo rm /tmp/ivshmem*
sudo pkill -9 qemu
tmux kill-server