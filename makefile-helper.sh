#!/usr/bin/zsh

SUPERUSER=sudo

echo -ne 'I\ncreate-gpt.fdisk\nw\n' | fdisk build/root.iso
$SUPERUSER losetup -f build/root.iso
LOOPBACK_DEVICE=`losetup -a | grep build/root.iso | awk -F: '{print $1;}'`
$SUPERUSER partprobe $LOOPBACK_DEVICE
ls $LOOPBACK_DEVICE*
$SUPERUSER mkfs.ext2 $LOOPBACK_DEVICE'p1'
$SUPERUSER mount $LOOPBACK_DEVICE'p1' mnt_root/
$SUPERUSER cp build/root/* mnt_root/
$SUPERUSER umount mnt_root/
$SUPERUSER losetup -d $LOOPBACK_DEVICE
