# Stramash AE

In popcorn\_kernel  and stramash\_kernel have the kernel source code. 
stramash-qemu is Stramash QEMU
  
## How to Run

#### 1. Set the root DIR of Stramash
```bash
cd Stramash-AE
STRAMASH_ROOT=$(pwd)
```
#### 2. Build the Docker for Compiling
```bash
cd $STRAMASH_ROOT/docker

# Create Compiling Env
sudo docker build -t stramash_env .

# Run Container
sudo docker run -dit --privileged --name stramash_container --mount type=bind,source="$(STRAMASH_ROOT)",target="$(STRAMASH_ROOT)" stramash_env

# Exec
sudo docker exec -it -w "$(STRAMASH_ROOT)" stramash_container /bin/bash

# To Stop it
# sudo docker stop stramash_container
# sudo docker rm stramash_container
```
#### 3. Build Kernel, file system, and Qemu
```bash
# Run inside the container

# Build X86 and Arm File System
./build_fs.sh
# Build Popcorn and Stramash Kernel
./build_kernel.sh
# Build qemu
./build_qemu.sh
exit
```
#### 4. Set up the Kernel and file system
```bash
sudo ./set_up.sh 
```
#### 5. Start Stramash
```bash
sudo ./start.sh # Will start 3 Machines
sudo ./end.sh
```
#### 6. Run NPB Benchmarks **Todo**
```bash
# User: Root
# Passward: stramash

For Stramash, both kernels need insert the module 
We have 3 different memory models, once the start opens, it opens 3 pair of machines in 3 windows with 1 tmux session
First is the Stramash Shard model, Second is the Stramash Separated model, and Third is the SHM

In each tmux window, you see 2 machines, left is x86,and  right is arm, or you can use $(uname -a) to check
Remember depends on speed, usually x86 is slower since it has more instructions, so insert the module on x86 side first, wait for 3sec, insert arm module

# For first 2 Stramash Machines pair run
insmod stramash_msg_shm.ko
# For last SHM Machines pair run
insmod shm_msg_shm.ko

Please run following command.

# Run NPB benchmark
$BIN could be one of the 4 (cg/is/ft/mg)
cd ./NPB_AE/$BIN;cat /proc/cache_sync_switch;cat /proc/popcorn_icount_switch;
```


#### 7. Check the Results 
```bash
EXAMPLE RESULT shows below


Both Shared and Separated model
Final Runtime = x86 Runtime + arm Runtime

For the Fully Shared model, because there is no remote access, we can just minus the feedback instruction from our cache model
Fully Shared = Final Runtime - Remote Memory Hits  * 0.455 (Either use the result from Separated model or Shared model) 

0.455 is the difference between remote access and local access.
Check the file  ->


For SHM, it is the same, however, because only the access to the message ring will be counted as remote access
Final Runtime = x86 Runtime + arm Runtime
Thus, SHM Fully Shared = Final Runtime - Remote Memory Hits  * 0.455
Same since fully shared doesn't have remote access. 

SHM Separated = Final Runtime - Remote Memory Hits(arm) * 0.455
Because in separated model, the x86 access to the shared memory ring is local, while it is expose to arm through sim CXL, so we consider arm access will be remote access

For Shared model, both access to ring buffer will consider as remote, just  => x86 Runtime + arm Runtime

```
![1ce570995205f6cba0bf2e43502fb43](https://github.com/user-attachments/assets/0a496074-2221-4b9a-8fbc-352ef0180740)

