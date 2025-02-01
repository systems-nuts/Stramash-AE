#!/bin/bash
ps aux | grep './ivsh-server.sh $1' | grep -v grep | awk '{print $2}' | xargs kill -9 &> /dev/null
ps aux | grep './cross_serv.sh $1' | grep -v grep | awk '{print $2}' | xargs kill -9 &> /dev/null
sudo rm /tmp/ivshmem_socket$1
sudo rm /tmp/cross_ipi_chr$1

./ivsh-server.sh $1 &> /dev/null &
./cross_serv.sh $1 &> /dev/null &