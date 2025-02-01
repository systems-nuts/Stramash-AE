#!/bin/bash

SERVER=cong@pluto

# rsync -avz $SERVER:/home/cong/qemu-stramash-8.0.0/startup/shm_version/popcorn_arm/arch/arm64/boot/Image ./kernel/arm/Image_Stramash
# rsync -avz $SERVER:/home/cong/qemu-stramash-8.0.0/startup/shm_version/popcorn_x86/arch/x86/boot/bzImage ./kernel/x86/Image_Stramash
# rsync -avz $SERVER:/home/cong/qemu-stramash-8.0.0/startup/shm_version/popcorn_x86/msg_layer/msg_shm.ko ./kernel/x86/stramash_msg_shm.ko
# rsync -avz $SERVER:/home/cong/qemu-stramash-8.0.0/startup/shm_version/popcorn_arm/msg_layer/msg_shm.ko ./kernel/arm/stramash_msg_shm.ko


# scp -r $SERVER:/home/cong/qemu-stramash-8.0.0/startup/shm_version/popcorn_arm/arch/arm64/boot/Image ./kernel/arm/Image_Stramash_opt
# scp -r $SERVER:/home/cong/qemu-stramash-8.0.0/startup/shm_version/popcorn_x86/arch/x86/boot/bzImage ./kernel/x86/Image_Stramash_opt
# scp -r $SERVER:/home/cong/qemu-stramash-8.0.0/startup/shm_version/popcorn_x86/msg_layer/msg_shm.ko ./kernel/x86/stramash_msg_shm_opt.ko
# scp -r $SERVER:/home/cong/qemu-stramash-8.0.0/startup/shm_version/popcorn_arm/msg_layer/msg_shm.ko ./kernel/arm/stramash_msg_shm_opt.ko

scp -r $SERVER:/home/cong/qemu-stramash-8.0.0/startup/shm_version/popcorn_arm/arch/arm64/boot/Image ./kernel/arm/Image_Stramash_normal
scp -r $SERVER:/home/cong/qemu-stramash-8.0.0/startup/shm_version/popcorn_x86/arch/x86/boot/bzImage ./kernel/x86/Image_Stramash_normal
scp -r $SERVER:/home/cong/qemu-stramash-8.0.0/startup/shm_version/popcorn_x86/msg_layer/msg_shm.ko ./kernel/x86/stramash_msg_shm_normal.ko
scp -r $SERVER:/home/cong/qemu-stramash-8.0.0/startup/shm_version/popcorn_arm/msg_layer/msg_shm.ko ./kernel/arm/stramash_msg_shm_normal.ko

# cp -r /home/cong/backup/kernel_6-21/arm/Image_Stramash ./kernel/arm/Image_Stramash
# cp -r /home/cong/backup/kernel_6-21/x86/Image_Stramash ./kernel/x86/Image_Stramash
# cp -r /home/cong/backup/kernel_6-21/x86/stramash_msg_shm.ko ./kernel/x86/stramash_msg_shm.ko
# cp -r /home/cong/backup/kernel_6-21/arm/stramash_msg_shm.ko ./kernel/arm/stramash_msg_shm.ko

# scp -r  $SERVER:/home/cong/qemu-stramash/startup/shm_version/popcorn_arm/arch/arm64/boot/Image ./kernel/arm/Image_Stramash_normal
# scp -r  $SERVER:/home/cong/qemu-stramash/startup/shm_version/popcorn_x86/arch/x86/boot/bzImage ./kernel/x86/Image_Stramash_normal
# scp -r  $SERVER:/home/cong/qemu-stramash/startup/shm_version/popcorn_x86/msg_layer/msg_shm.ko ./kernel/x86/stramash_msg_shm_normal.ko
# scp -r  $SERVER:/home/cong/qemu-stramash/startup/shm_version/popcorn_arm/msg_layer/msg_shm.ko ./kernel/arm/stramash_msg_shm_normal.ko

# rsync -avz $SERVER:/home/cong/qemu-stramash-8.0.0/startup/ipi_version_futex/popcorn_arm/arch/arm64/boot/Image ./kernel/arm/Image_TCP
# rsync -avz $SERVER:/home/cong/qemu-stramash-8.0.0/startup/ipi_version_futex/popcorn_x86/arch/x86/boot/bzImage ./kernel/x86/Image_TCP
# rsync -avz $SERVER:/home/cong/qemu-stramash-8.0.0/startup/ipi_version_futex/popcorn_x86/msg_layer/msg_shm.ko ./kernel/x86/tcp_msg_shm.ko
# rsync -avz $SERVER:/home/cong/qemu-stramash-8.0.0/startup/ipi_version_futex/popcorn_arm/msg_layer/msg_shm.ko ./kernel/arm/tcp_msg_shm.ko

# rsync -avz $SERVER:/home/tong/UCSB/NO_MSG/arm64-Image ./kernel/arm/Image_Stramash_prev
# rsync -avz $SERVER:/home/tong/UCSB/NO_MSG/x86_64-Image ./kernel/x86/Image_Stramash_prev
# rsync -avz $SERVER:/home/tong/UCSB/NO_MSG/msg_x86.ko ./kernel/x86/stramash_msg_shm_prev.ko
# rsync -avz $SERVER:/home/tong/UCSB/NO_MSG/msg_arm.ko ./kernel/arm/stramash_msg_shm_prev.ko

 
# rsync -avz $SERVER:/home/tong/UCSB/SHM_MSG/arm64-Image ./kernel/arm/Image_TCP_prev
# rsync -avz $SERVER:/home/tong/UCSB/SHM_MSG/x86_64-Image ./kernel/x86/Image_TCP_prev
# rsync -avz $SERVER:/home/tong/UCSB/SHM_MSG/x86_msg.ko ./kernel/x86/tcp_msg_shm_prev.ko
# rsync -avz $SERVER:/home/tong/UCSB/SHM_MSG/arm_msg.ko  ./kernel/arm/tcp_msg_shm_prev.ko

# rsync -avz $SERVER:/home/cong/qemu-stramash-8.0.0/startup/ipi_version_futex/popcorn_arm/arch/arm64/boot/Image ./kernel/arm/Image_SHM
# rsync -avz $SERVER:/home/cong/qemu-stramash-8.0.0/startup/ipi_version_futex/popcorn_x86/arch/x86/boot/bzImage ./kernel/x86/Image_SHM
# rsync -avz $SERVER:/home/cong/qemu-stramash-8.0.0/startup/ipi_version_futex/popcorn_x86/msg_layer/msg_shm.ko ./kernel/x86/shm_msg_shm.ko
# rsync -avz $SERVER:/home/cong/qemu-stramash-8.0.0/startup/ipi_version_futex/popcorn_arm/msg_layer/msg_shm.ko ./kernel/arm/shm_msg_shm.ko

# for i in {1..9}; do

# sudo umount temp_mnt1 &> /dev/null
# sudo umount temp_mnt2 &> /dev/null

# sudo mount x86_rootfs${i}.img temp_mnt1
# sudo mount arm_rootfs${i}.img temp_mnt2

# sudo cp -rp stramash_cong temp_mnt1/root
# sudo cp -rp stramash_cong temp_mnt2/root

# sudo umount temp_mnt1
# sudo umount temp_mnt2
# done
