#!/bin/bash
sudo pkill -9 qemu
./update.sh 1
./close_daemon.sh &> /dev/null
./plugin_clean.sh &> /dev/null


for i in $(seq 1 $1); do
    ./ivsh-server.sh $i &> /dev/null &
    ./cross_serv.sh $i &> /dev/null &
done
