#!/usr/bin/bash

echo -ne 'I\ncreate-gpt.fdisk\nw\n' | fdisk build/root.iso
sudo losetup -f build/root.iso
LOOPBACK_DEVICE=`losetup -a | grep build/root.iso | awk -F: '{print $1;}'`
sudo partprobe $LOOPBACK_DEVICE
ls $LOOPBACK_DEVICE*
sudo mkfs.ext2 $LOOPBACK_DEVICE'p1'
sudo mount $LOOPBACK_DEVICE'p1' mnt_root/
sudo cp build/root/* mnt_root/
sudo umount mnt_root/
sudo losetup -d $LOOPBACK_DEVICE
