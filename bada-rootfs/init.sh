#!/bin/sh
#
# Pie Kernel - Bada OS Init Script
# Mounts essential filesystems and starts OSP framework
#
# Copyright (C) 2026 Bada OS Reconstruction Project

echo "Pie Kernel: Starting Bada OS init..."

# Mount essential filesystems
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t tmpfs tmpfs /tmp
mount -t tmpfs tmpfs /dev

# Create essential device nodes
mkdir -p /dev/pts
mount -t devpts devpts /dev/pts

# Create basic device nodes
mknod /dev/null    c 1 3
mknod /dev/zero    c 1 5
mknod /dev/random  c 1 8
mknod /dev/urandom c 1 9
mknod /dev/tty     c 5 0
mknod /dev/tty0    c 4 0
mknod /dev/ttyMSM0 c 252 0
mknod /dev/fb0     c 29 0
mknod /dev/input/event0 c 13 64
mknod /dev/input/event1 c 13 65
mknod /dev/input/event2 c 13 66
mknod /dev/input/event3 c 13 67

# Create Bada-specific directories
mkdir -p /SystemFS/User/OspSys/registry
mkdir -p /SystemFS/Media/Images
mkdir -p /SystemFS/Media/Sounds
mkdir -p /AppEx
mkdir -p /tmp/osp

# Set permissions
chmod 755 /proc
chmod 755 /sys
chmod 1777 /tmp
chmod 755 /dev

echo "Pie Kernel: Filesystems mounted"

# Start OSP framework
echo "Pie Kernel: Starting OSP framework..."
exec /sbin/osp_init