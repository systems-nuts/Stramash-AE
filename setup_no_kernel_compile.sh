#!/bin/bash
stramash=./stramash_kernel
popcorn=./popcorn_kernel
target=stramash-qemu/startup

echo "Copy pre-compiled kernel..."

cp -r $target/EASY-LIFE-AE $target/kernel


echo "Copy FS..."
mv rootfs-arm64.img $target/arm_rootfs1.img &> /dev/null
mv rootfs-amd64.img $target/x86_rootfs1.img &> /dev/null

echo "Copy NPB benchmarks..."

cd $target
mkdir -p ./temp_mnt1
mkdir -p ./temp_mnt2

sudo umount temp_mnt1 &> /dev/null
sudo umount temp_mnt2 &> /dev/null

sudo mount x86_rootfs1.img temp_mnt1
sudo mount arm_rootfs1.img temp_mnt2

sudo cp -p ./kernel/x86/*.ko temp_mnt1/root
sudo cp -p ./kernel/arm/*.ko temp_mnt2/root

# /////////////////////////////////////////////
for file in ./NPB_AE/*_x86-64; do
  [ -e "$file" ] || continue

  new_file="${file%_x86-64}"

  cp "$file" "$new_file"

  echo "Copied $file to $new_file"

done
sudo cp -rp ./NPB_AE temp_mnt1/root/
for file in ./NPB_AE/*_aarch64; do
  [ -e "$file" ] || continue

  new_file="${file%_aarch64}"

  cp "$file" "$new_file"

  echo "Copied $file to $new_file"

done
sudo cp -rp ./NPB_AE temp_mnt2/root/
# /////////////////////////////////////////////
sudo umount temp_mnt1 &> /dev/null
sudo umount temp_mnt2 &> /dev/null

rm -r ./temp_mnt1
rm -r ./temp_mnt2


cd -
echo finished.
