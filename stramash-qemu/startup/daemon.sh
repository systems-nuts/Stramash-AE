#!/bin/bash
sudo pkill -9 qemu
./update.sh 1
./close_daemon.sh
./plugin_clean.sh


for i in $(seq 1 $1); do
    ./ivsh-server.sh $i &> ./logs/logi_$i&
    ./cross_serv.sh $i &> ./logs/logc_$i&
done
