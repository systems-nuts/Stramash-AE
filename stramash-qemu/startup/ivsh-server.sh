#!/bin/bash
../build/contrib/ivshmem-server/ivshmem-server -M ivshmem$1 -p /var/run/ivshmem-server$1.pid -S /tmp/ivshmem_socket$1 -vF -l 4K -n 1
