#!/bin/bash
pkill ivsh-server.sh
pkill cross_serv.sh
sudo pkill ivshmem-server
sudo rm /tmp/cross_ipi_chr* &> /dev/null
sudo rm /tmp/ivshmem_socket* &> /dev/null
