#!/bin/bash
sudo rm /dev/shm/*$1 &>/dev/null
sudo rm /tmp/cross*$1 &>/dev/null
sudo rm /tmp/ivshmem*$1 &>/dev/null

