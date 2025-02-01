#!/bin/bash

process1="cpu_background_arm.out"
process2="cpu_background_x86.out"

if pgrep -f "$process1" > /dev/null || pgrep -f "$process2" > /dev/null; then
  echo "One of the processes ($process1 or $process2) is already running. Exiting without running cpu_background_arm.out."
else
  echo "Neither $process1 nor $process2 is running. Running cpu_background_x86.out with the lowest priority."
  nice -n 19 /root/cpu_background_arm.out &
fi

