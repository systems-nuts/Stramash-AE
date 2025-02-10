# Stramash AE

In popcorn\_kernel  and stramash\_kernel have the kernel source code. 
stramash-qemu is Stramash QEMU
  
## How to Run

#### 1. Set the root DIR of Stramash
```bash
cd Stramash-AE
STRAMASH_ROOT=$(pwd)
```
#### 2. Build the Docker for Compiling (For Cloud VM, please direct lunch a ubuntu22.06 instance and run everything out side is OK, please just copy the apt get part inside Dockerfile.
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
#### 3. Build Kernel(OPTIONAL), file system, and Qemu 
```bash
# Run inside the container

# Build X86 and Arm File System
./build_fs.sh
# Build Popcorn and Stramash Kernel
./build_kernel.sh (OPTIONAL, we have provided a pre-built image and kernel module) 
# Build qemu
./build_qemu.sh
exit the docker container. now back to the host env ->!!!exit docker container!!!
Remember, the Dockerfile is only for helping build the kernel and QEMU.
You can also build it locally if you have ubuntu22.04, you can just check all the step at Dockerfile
For QEMU there are some lib needs. Please consider also running the apt-get install command in the docker/Dockerfile
But it depends on your machine's environment. 

```
#### 4. Set up the Kernel and file system
```bash
(IF Kernel compiled before)
sudo ./set_up.sh
--------------------------------------------
(IF NO Kernel compiled before)
chmod +x setup_no_kernel_compile.sh
sudo ./setup_no_kernel_compile.sh
```
#### 5. Start Stramash
```bash
sudo ./start.sh # Will start 3 pairs of Stramash Machines
sudo ./end.sh # Use for help turn off all the Stramash Machine 
```
#### 6. Run NPB Benchmarks 
```bash
# User: Root
# Passward: stramash

Now the 3 pairs of Stramash Machines are running inside 3 pairs of TMUX windows in 1 session
Inside the Stramash-QEMU now... Please run everything inside the QEMU
For Stramash, both kernels need to insert the module 
We have 3 different memory models, once the start opens, it opens 3 pairs of machines in 3 windows with 1 tmux session
First is the Stramash Shard model, Second is the Stramash Separated model, and Third is the SHM

In each tmux window, you see 2 machines, left is x86, and right is arm, or you can use $(uname -a) to check
Remember depends on speed, usually x86 is slower since it has more instructions,
so insert the module on x86 side first, wait for 3sec, insert arm module

# For first 2 Stramash Machines pair run
insmod stramash_msg_shm.ko
# For last SHM Machines pair run
insmod shm_msg_shm.ko

Please run the following command.

# Run NPB benchmark, each took hours on a strong core.
The binary is located at stramash-qemu/startup/NPB_AE on the host
our script will help you to copy everything inside here into QEMU machines  -- there are many helper scripts as well.

$BIN could be one of the 4 (cg/is/ft/mg)
inside qemu run the following command on ARM, the binary will migrate to arm in the middle, and migrate back upon finished(because only x86 can do print)
./NPB_AE/$BIN;cat /proc/cache_sync_switch;cat /proc/popcorn_icount_switch;
```


#### 7. Check the Results 
```bash
EXAMPLE RESULT shown below

The EXPERIMENT WE ARE EVALUATION IS: Figure 9. NPB benchmark results

Both Shared and Separated model
Final Runtime = x86 Runtime + arm Runtime

For the Fully Shared model, because there is no remote access, we can approximate it by 
 minus the feedback instruction from our cache model
But because the Fully Shared model uses shared L3, to get precise results,
please reconfigure the Cache-Plugin at stramash-qemu/contrib/plugins/cache-sim-feedback.c at L2328 and L2350
and recompile the QEMU at https://github.com/systems-nuts/Stramash-AE/blob/main/stramash-qemu/contrib/plugins/cache-sim-feedback.c

Fully Shared = Final Runtime - Remote Memory Hits  * 0.455 (Either use the result from the Separated model or Shared model) 
0.455 is the difference between remote access and local access.
Check the file  -> https://github.com/systems-nuts/Stramash-AE/blob/main/stramash-qemu/contrib/plugins/cache-sim-feedback.c#L215
#define Local_mem_overhead 360
#define Remote_mem_overhead 660
660/360 => 0.455
We use this to reduce the Machine to run.


For SHM, it is the same, however, because only the access to the message ring will be counted as remote access
Final Runtime = x86 Runtime + arm Runtime
Thus, SHM Fully Shared = Final Runtime - Remote Memory Hits  * 0.455
Same since fully shared doesn't have remote access. 

SHM Separated = Final Runtime - Remote Memory Hits(only x86) * 0.455
Because in the separated model, the x86 access to the shared memory ring is local,
while it is exposed to the arm through sim CXL, so we consider arm access to be remote access,
so the total runtime will be minus the Remote Memory Hits of x86, which we consider to be local now.

For the Shared model, both access to the ring buffer will be considered as remote, just  => x86 Runtime + arm Runtime

```
![1ce570995205f6cba0bf2e43502fb43](https://github.com/user-attachments/assets/0a496074-2221-4b9a-8fbc-352ef0180740)

