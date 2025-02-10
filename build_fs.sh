#!/bin/bash
set -e

DEBIAN_RELEASE="buster"
SIZE="5G"
DEBIAN_SRC="http://deb.debian.org/debian/"

ARCHES=("amd64" "arm64")

sudo apt-get update
sudo apt-get install -y qemu-user-static binfmt-support
sudo update-binfmts --enable qemu-aarch64

for ARCH in "${ARCHES[@]}"; do
  echo "============================================="
  echo "[*] Building Debian $DEBIAN_RELEASE for ARCH=$ARCH"
  echo "============================================="

  IMG="./rootfs-${ARCH}.img"
  DIR="./temp-${ARCH}"
  ROOTFS="./rootfs-${ARCH}"

  # ---------- Step 1: Clean up old environment ----------
  echo "[*] Creating temporary mount directory: $DIR"
  mkdir -p "$DIR"

  # 1.1 Unmount any residual mounts
  if mount | grep -q "$DIR"; then
    echo "[*] $DIR is already mounted, unmounting..."

    sudo umount "$DIR" 2>/dev/null || true
  fi
  if mount | grep -q "$ROOTFS"; then
    echo "[*] $ROOTFS is still mounted, unmounting..."
	sudo umount "$ROOTFS/proc" 2>/dev/null || true
    sudo umount "$ROOTFS/sys" 2>/dev/null || true
    sudo umount "$ROOTFS/dev" 2>/dev/null || true
    sudo umount "$ROOTFS" 2>/dev/null || true
  fi

  # 1.2 Detach any loop device bindings
  if losetup -a | grep -q "$IMG"; then
    echo "[*] Detected loop device for $IMG, detaching..."
    for loopdev in $(losetup -a | grep "$IMG" | cut -d: -f1); do
      sudo losetup -d "$loopdev"
    done
  fi

  # 1.3 Remove old image file
  if [ -f "$IMG" ]; then
    echo "[*] Removing old $IMG..."
    rm -f "$IMG"
  fi

  # ---------- Step 2: Create and format a new image ----------
  echo "[*] Creating disk image $IMG with size $SIZE..."
  qemu-img create "$IMG" "$SIZE"

  echo "[*] Formatting the image as ext4..."
  mkfs.ext4 -F "$IMG"

  # ---------- Step 3: Mount the image to $DIR and run debootstrap ----------
  echo "[*] Mounting $IMG to $DIR..."
  sudo mount -o loop "$IMG" "$DIR"

  if [ "$ARCH" = "arm64" ]; then
    echo "[*] Installing minimal Debian ($DEBIAN_RELEASE) for ARCH=$ARCH (foreign stage)..."
    sudo debootstrap --foreign --arch="$ARCH" "$DEBIAN_RELEASE" "$DIR" "$DEBIAN_SRC"
  else
    echo "[*] Installing minimal Debian ($DEBIAN_RELEASE) for ARCH=$ARCH..."
    sudo debootstrap --arch="$ARCH" "$DEBIAN_RELEASE" "$DIR" "$DEBIAN_SRC"
  fi

  echo "[*] Unmounting temporary mount $DIR..."
  sudo umount "$DIR"
  rm -rf "$DIR"

  # ---------- Step 4: Bind loop device and mount to $ROOTFS ----------
  echo "[*] Binding image to loop device..."
  LOOP_DEVICE=$(sudo losetup --partscan --show --find "$IMG")
  echo "    Bound loop device: $LOOP_DEVICE"

  echo "[*] Creating and mounting $ROOTFS..."
  mkdir -p "$ROOTFS"
  sudo mount "${LOOP_DEVICE}" "$ROOTFS"

  if [ "$ARCH" = "arm64" ]; then
    echo "[*] Copying qemu-aarch64-static into chroot..."
    sudo cp /usr/bin/qemu-aarch64-static "$ROOTFS/usr/bin/"
  fi

  # ---------- Step 5: Bind system directories and enter chroot ----------
  echo "[*] Binding system directories (proc, sys, dev) into chroot..."
  sudo mount -B /proc "$ROOTFS/proc"
  sudo mount -B /sys  "$ROOTFS/sys"
  sudo mount -B /dev  "$ROOTFS/dev"

  echo "[*] Entering chroot to set root password and perform second-stage setup if needed..."
  cd "$ROOTFS"
  sudo chroot . /bin/bash <<EOF
if [ "$ARCH" = "arm64" ]; then
  /debootstrap/debootstrap --second-stage
fi
apt-get update
apt-get install -y passwd apt net-tools

echo "root:stramash" | chpasswd
EOF
  cd -

  # ---------- Step 6: Clean up and unmount ----------
  echo "[*] Unmounting system directories..."
  sudo umount "$ROOTFS/proc" 2>/dev/null || true
  sudo umount "$ROOTFS/sys" 2>/dev/null || true
  sudo umount "$ROOTFS/dev" 2>/dev/null || true
  sudo umount "$ROOTFS" 2>/dev/null || true

  echo "[*] Detaching loop device..."
  sudo losetup -d "$LOOP_DEVICE"

  echo "[*] Removing chroot directory $ROOTFS..."
  rm -rf "$ROOTFS"

  echo "[*] Done building $IMG for ARCH=$ARCH."
  echo
  
done

echo "[Finished] Debian $DEBIAN_RELEASE images created for: ${ARCHES[*]}"
echo "Check ./ for rootfs-amd64.img and rootfs-arm64.img."
