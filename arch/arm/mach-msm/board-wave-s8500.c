/*
 * Pie Kernel - Samsung Wave S8500 board support
 * Based on board-msm7x27.c
 * Copyright (C) 2024 Bada OS Reconstruction Project
 *
 * Hardware:
 * - CPU: Qualcomm MSM7227 ARM Cortex-A8 800MHz
 * - RAM: 256MB
 * - WiFi: Broadcom BCM4329
 * - Audio: Wolfson WM8994
 * - Camera: Samsung CE147
 * - Display: AMOLED 480x800
 * - Touchscreen: Synaptics
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <mach/hardware.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <mach/board.h>
#include <mach/msm_iomap.h>
#include "devices.h"
#include "clock.h"

/* BCM4329 WiFi */
static struct platform_device bcm4329_wifi_device = {
    .name = "bcm4329_wlan",
    .id   = -1,
};

/* WM8994 Audio */
static struct platform_device wm8994_audio_device = {
    .name = "wm8994-codec",
    .id   = -1,
};

static struct platform_device *wave_s8500_devices[] __initdata = {
    &bcm4329_wifi_device,
    &wm8994_audio_device,
};

static void __init wave_s8500_init(void)
{
    platform_add_devices(wave_s8500_devices,
                        ARRAY_SIZE(wave_s8500_devices));
}

static void __init wave_s8500_map_io(void)
{
    msm_map_common_io();
}

MACHINE_START(WAVE_S8500, "Samsung Wave S8500")
    .atag_offset = 0x100,
    .map_io      = wave_s8500_map_io,
    .init_machine = wave_s8500_init,
MACHINE_END