/*
 * Pie Kernel - Samsung Wave S8500 AMOLED Display Driver
 * Samsung Wave S8500 uses AMOLED 480x800 display
 *
 * Copyright (C) 2024 Bada OS Reconstruction Project
 * GPL v2
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fb.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>

#define WAVE_FB_NAME        "wave_s8500_fb"
#define WAVE_SCREEN_WIDTH   480
#define WAVE_SCREEN_HEIGHT  800
#define WAVE_BITS_PER_PIXEL 16
#define WAVE_BYTES_PER_LINE (WAVE_SCREEN_WIDTH * WAVE_BITS_PER_PIXEL / 8)
#define WAVE_FB_SIZE        (WAVE_BYTES_PER_LINE * WAVE_SCREEN_HEIGHT)

/* AMOLED controller registers */
#define AMOLED_REG_POWER    0x00
#define AMOLED_REG_MODE     0x01
#define AMOLED_REG_WIDTH    0x02
#define AMOLED_REG_HEIGHT   0x03
#define AMOLED_REG_REFRESH  0x04
#define AMOLED_REG_BRIGHTNESS 0x05

struct wave_fb {
    struct fb_info      *info;
    void                *fb_mem;
    dma_addr_t          fb_dma;
    void __iomem        *regs;
};

static struct fb_var_screeninfo wave_fb_var = {
    .xres           = WAVE_SCREEN_WIDTH,
    .yres           = WAVE_SCREEN_HEIGHT,
    .xres_virtual   = WAVE_SCREEN_WIDTH,
    .yres_virtual   = WAVE_SCREEN_HEIGHT,
    .bits_per_pixel = WAVE_BITS_PER_PIXEL,
    .red            = { 11, 5, 0 },
    .green          = { 5,  6, 0 },
    .blue           = { 0,  5, 0 },
    .transp         = { 0,  0, 0 },
    .activate       = FB_ACTIVATE_NOW,
    .width          = 56,   /* physical width in mm */
    .height         = 93,   /* physical height in mm */
    .pixclock       = 16129,
    .left_margin    = 8,
    .right_margin   = 16,
    .upper_margin   = 2,
    .lower_margin   = 16,
    .hsync_len      = 8,
    .vsync_len      = 2,
    .vmode          = FB_VMODE_NONINTERLACED,
};

static struct fb_fix_screeninfo wave_fb_fix = {
    .id             = WAVE_FB_NAME,
    .type           = FB_TYPE_PACKED_PIXELS,
    .visual         = FB_VISUAL_TRUECOLOR,
    .line_length    = WAVE_BYTES_PER_LINE,
    .accel          = FB_ACCEL_NONE,
};

static int wave_fb_check_var(struct fb_var_screeninfo *var,
                              struct fb_info *info)
{
    if (var->xres != WAVE_SCREEN_WIDTH ||
        var->yres != WAVE_SCREEN_HEIGHT)
        return -EINVAL;

    if (var->bits_per_pixel != 16 &&
        var->bits_per_pixel != 32)
        return -EINVAL;

    return 0;
}

static int wave_fb_set_par(struct fb_info *info)
{
    struct wave_fb *fb = info->par;

    /* Set display resolution */
    writeb(WAVE_SCREEN_WIDTH >> 8,
           fb->regs + AMOLED_REG_WIDTH);
    writeb(WAVE_SCREEN_HEIGHT >> 8,
           fb->regs + AMOLED_REG_HEIGHT);

    /* Set refresh rate to 60Hz */
    writeb(60, fb->regs + AMOLED_REG_REFRESH);

    return 0;
}

static int wave_fb_setcolreg(unsigned regno, unsigned red,
                              unsigned green, unsigned blue,
                              unsigned transp, struct fb_info *info)
{
    u32 *pal = info->pseudo_palette;

    if (regno >= 16)
        return -EINVAL;

    pal[regno] = (red   & 0xF800) |
                 ((green & 0xFC00) >> 5) |
                 ((blue  & 0xF800) >> 11);
    return 0;
}

static int wave_fb_blank(int blank, struct fb_info *info)
{
    struct wave_fb *fb = info->par;

    switch (blank) {
    case FB_BLANK_UNBLANK:
        writeb(0x01, fb->regs + AMOLED_REG_POWER);
        break;
    case FB_BLANK_POWERDOWN:
        writeb(0x00, fb->regs + AMOLED_REG_POWER);
        break;
    default:
        break;
    }
    return 0;
}

static struct fb_ops wave_fb_ops = {
    .owner          = THIS_MODULE,
    .fb_check_var   = wave_fb_check_var,
    .fb_set_par     = wave_fb_set_par,
    .fb_setcolreg   = wave_fb_setcolreg,
    .fb_blank       = wave_fb_blank,
    .fb_fillrect    = cfb_fillrect,
    .fb_copyarea    = cfb_copyarea,
    .fb_imageblit   = cfb_imageblit,
};

static int wave_fb_probe(struct platform_device *pdev)
{
    struct wave_fb *fb;
    struct fb_info *info;
    int ret;

    info = framebuffer_alloc(sizeof(*fb), &pdev->dev);
    if (!info)
        return -ENOMEM;

    fb = info->par;

    /* Allocate framebuffer memory */
    fb->fb_mem = dma_alloc_coherent(&pdev->dev, WAVE_FB_SIZE,
                                     &fb->fb_dma, GFP_KERNEL);
    if (!fb->fb_mem) {
        framebuffer_release(info);
        return -ENOMEM;
    }

    /* Clear framebuffer */
    memset(fb->fb_mem, 0, WAVE_FB_SIZE);

    info->fbops        = &wave_fb_ops;
    info->var          = wave_fb_var;
    info->fix          = wave_fb_fix;
    info->fix.smem_start = fb->fb_dma;
    info->fix.smem_len   = WAVE_FB_SIZE;
    info->screen_base    = fb->fb_mem;
    info->screen_size    = WAVE_FB_SIZE;
    info->pseudo_palette = kzalloc(sizeof(u32) * 16, GFP_KERNEL);

    ret = register_framebuffer(info);
    if (ret) {
        dma_free_coherent(&pdev->dev, WAVE_FB_SIZE,
                          fb->fb_mem, fb->fb_dma);
        framebuffer_release(info);
        return ret;
    }

    platform_set_drvdata(pdev, info);
    dev_info(&pdev->dev, "Wave S8500 AMOLED 480x800 initialized\n");
    return 0;
}

static int wave_fb_remove(struct platform_device *pdev)
{
    struct fb_info *info = platform_get_drvdata(pdev);
    struct wave_fb *fb = info->par;

    unregister_framebuffer(info);
    dma_free_coherent(&pdev->dev, WAVE_FB_SIZE,
                      fb->fb_mem, fb->fb_dma);
    kfree(info->pseudo_palette);
    framebuffer_release(info);
    return 0;
}

static struct platform_driver wave_fb_driver = {
    .probe  = wave_fb_probe,
    .remove = wave_fb_remove,
    .driver = {
        .name  = WAVE_FB_NAME,
        .owner = THIS_MODULE,
    },
};

module_platform_driver(wave_fb_driver);

MODULE_AUTHOR("Bada OS Reconstruction Project");
MODULE_DESCRIPTION("Samsung Wave S8500 AMOLED Display Driver");
MODULE_LICENSE("GPL v2");