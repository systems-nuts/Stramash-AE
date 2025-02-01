#!/bin/bash
../build/contrib/ivshmem-server/ivshmem-server -p /var/run/ivshmem-server$1.pid -M ivshmem$1 -S /tmp/cross_ipi_chr$1 -v -F l 0 -n 1
