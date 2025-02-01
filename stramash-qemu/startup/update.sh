#!/bin/bash
sudo umount temp_mnt1 &> /dev/null
sudo umount temp_mnt2 &> /dev/null

sudo mount x86_rootfs$1.img temp_mnt1
sudo mount arm_rootfs$1.img temp_mnt2

# sudo cp -p ./shm_version/popcorn_x86/msg_layer/msg_shm.ko temp_mnt1/root/shm_msg_layer.ko
# sudo cp -p ./shm_version/popcorn_arm/msg_layer/msg_shm.ko temp_mnt2/root/shm_msg_layer.ko

# sudo cp -p ./ipi_version/popcorn_x86/msg_layer/msg_shm.ko temp_mnt1/root/ipi_msg_layer.ko
# sudo cp -p ./ipi_version/popcorn_arm/msg_layer/msg_shm.ko temp_mnt2/root/ipi_msg_layer.ko

sudo cp -p ./run_x86cpu.sh temp_mnt1/root
sudo cp -p ./run_armcpu.sh temp_mnt2/root

sudo cp -p ./kernel/x86/*.ko temp_mnt1/root
sudo cp -p ./kernel/arm/*.ko temp_mnt2/root

sudo cp -rp ./malloc_for_cong/ temp_mnt1/root/
sudo cp -rp ./malloc_for_cong/ temp_mnt2/root/
sudo cp -rp ./malloc_for_cong/malloc_x86-64 temp_mnt1/root/malloc_for_cong/malloc
sudo cp -rp ./malloc_for_cong/malloc_aarch64 temp_mnt2/root/malloc_for_cong/malloc
#//////////////////////////////////////////////
for file in ./malloc_last/local/*_x86-64; do
  [ -e "$file" ] || continue
  
  new_file="${file%_x86-64}"
  
  cp "$file" "$new_file"
  
  echo "Copied $file to $new_file"

done
for file in ./malloc_last/remote/*_x86-64; do
  [ -e "$file" ] || continue
  
  new_file="${file%_x86-64}"
  
  cp "$file" "$new_file"
  
  echo "Copied $file to $new_file"

done
sudo cp -rp ./malloc_last temp_mnt1/root/
for file in ./malloc_last/local/*_aarch64; do
  [ -e "$file" ] || continue
  
  new_file="${file%_aarch64}"
  
  cp "$file" "$new_file"
  
  echo "Copied $file to $new_file"

done
for file in ./malloc_last/remote/*_aarch64; do
  [ -e "$file" ] || continue
  
  new_file="${file%_aarch64}"
  
  cp "$file" "$new_file"
  
  echo "Copied $file to $new_file"

done
sudo cp -rp ./malloc_last temp_mnt2/root/

# /////////////////////////////////////////////
for file in ./NPB_final_621/*_x86-64; do
  [ -e "$file" ] || continue
  
  new_file="${file%_x86-64}"
  
  cp "$file" "$new_file"
  
  echo "Copied $file to $new_file"

done
sudo cp -rp ./NPB_final_621 temp_mnt1/root/
for file in ./NPB_final_621/*_aarch64; do
  [ -e "$file" ] || continue
  
  new_file="${file%_aarch64}"
  
  cp "$file" "$new_file"
  
  echo "Copied $file to $new_file"

done
sudo cp -rp ./NPB_final_621 temp_mnt2/root/
# /////////////////////////////////////////////
for file in ./futex_last/*_x86-64; do
  [ -e "$file" ] || continue
  
  new_file="${file%_x86-64}"
  
  cp "$file" "$new_file"
  
  echo "Copied $file to $new_file"

done
sudo cp -rp ./futex_last temp_mnt1/root/
for file in ./futex_last/*_aarch64; do
  [ -e "$file" ] || continue
  
  new_file="${file%_aarch64}"
  
  cp "$file" "$new_file"
  
  echo "Copied $file to $new_file"

done
sudo cp -rp ./futex_last temp_mnt2/root/
# for file in ./NPB_next/A/*_x86-64; do
#   [ -e "$file" ] || continue
  
#   new_file="${file%_x86-64}"
  
#   cp "$file" "$new_file"
# done
# sudo cp -rp ./NPB_next/A temp_mnt1/root/
# for file in ./NPB_next/A/*_aarch64; do
#   [ -e "$file" ] || continue
  
#   new_file="${file%_aarch64}"
  
#   cp "$file" "$new_file"
  
# done
# sudo cp -rp ./NPB_next/A temp_mnt2/root/

# /////////////////////////////////////////////
# sudo cp -rp ./NPB_no_mig temp_mnt1/root/
# sudo cp -rp ./NPB_no_mig temp_mnt2/root/
# sudo ls temp_mnt2/root/
# sudo cp -p ./stramash_cong/A/ft_x86-64 temp_mnt1/root/ft
# sudo cp -p ./stramash_cong/A/ft_aarch64 temp_mnt2/root/ft

sudo cp -p ./cpu_background_x86.out temp_mnt1/root/
sudo cp -p ./cpu_background_arm.out temp_mnt2/root/

sudo rm  temp_mnt1/root/cpu_background_x86.out
sudo rm  temp_mnt2/root/cpu_background_arm.out 

# sudo cp -p ./futex_* temp_mnt1/root/
# sudo cp -p ./futex_* temp_mnt2/root/

sudo umount temp_mnt1
sudo umount temp_mnt2

sudo fsck.ext4 -p x86_rootfs$1.img
sudo fsck.ext4 -p arm_rootfs$1.img



