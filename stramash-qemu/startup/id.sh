#!/bin/bash
ls /tmp/cross_arch_ipi_fifo* | grep -o '[0-9]\+' | sort -n | tail -n1
