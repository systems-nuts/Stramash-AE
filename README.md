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
#### 6. Run NPB Benchmarks
<span style="color:red;">需要完善</span>
```bash
# User: Root
# Passward: stramash

For Stramash, both kernel need insert the 
# Both Machines run
insmod stramash_msg_shm.ko
# Both Machines run
insmod shm_msg_shm.ko

# Run cg
cd ./NPB_AE/cg;cat /proc/cache_sync_switch;cat /proc/popcorn_icount_switch;
# Run is
cd ./NPB_AE/is;cat /proc/cache_sync_switch;cat /proc/popcorn_icount_switch;
# Run ft
cd ./NPB_AE/ft;cat /proc/cache_sync_switch;cat /proc/popcorn_icount_switch;
# Run mg
cd ./NPB_AE/mg;cat /proc/cache_sync_switch;cat /proc/popcorn_icount_switch;
```


#### 7. Check the Results
Final Runtime = x86 Runtime + arm Runtime

Fully Shared = Final Runtime - Remote Memory Hits  * 0.455
![1ce570995205f6cba0bf2e43502fb43](https://github.com/user-attachments/assets/0a496074-2221-4b9a-8fbc-352ef0180740)

