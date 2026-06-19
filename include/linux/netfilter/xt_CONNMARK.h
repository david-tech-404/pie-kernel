#ifndef _XT_CONNMARK_H_target
#define _XT_CONNMARK_H_target

#include <linux/netfilter/xt_connmark.h>

#endif /*_XT_CONNMARK_H_target*/

# Filesystem support for OSP framework
CONFIG_EXT2_FS=y
CONFIG_EXT3_FS=y
CONFIG_TMPFS=y
CONFIG_PROC_FS=y
CONFIG_SYSFS=y

# Binary format - required to run Bada .exe (ELF ARM)
CONFIG_BINFMT_ELF=y
CONFIG_BINFMT_SHARED=y

# Unix sockets - required for mappserver IPC (Pickle/SyncMessage)
CONFIG_UNIX=y

# Full network stack for FNetPiServer.so
CONFIG_PACKET=y
CONFIG_INET=y
CONFIG_IP_MULTICAST=y
CONFIG_NET_TCP=y
CONFIG_NET_UDP=y
CONFIG_WIRELESS=y
CONFIG_CFG80211=y
CONFIG_WIRELESS_EXT=y

# Shared memory and IPC primitives
CONFIG_SYSVIPC=y
CONFIG_POSIX_MQUEUE=y

# Dynamic library loading (FOsp.so, mosp.so, etc need this)
CONFIG_BINFMT_ELF_FDPIC=n

# File locking for sqlite360.so
CONFIG_FILE_LOCKING=y

# RTC for SystemTime / DateTime APIs
CONFIG_RTC_CLASS=y
CONFIG_RTC_DRV_MSM=y