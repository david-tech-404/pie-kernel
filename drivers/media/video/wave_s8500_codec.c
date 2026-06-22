/*
 * Pie Kernel - Samsung Wave S8500 Video Codec Driver
 * H.264 and MPEG4 codec support for Bada OS media apps
 * Required by Bada OS FMediaPiServer.so
 *
 * Copyright (C) 2026 Bada OS Reconstruction Project
 * GPL v2
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/dma-mapping.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-core.h>

#define WAVE_CODEC_NAME     "wave_s8500_codec"

/* Codec types */
#define CODEC_H264          0
#define CODEC_MPEG4         1
#define CODEC_H263          2
#define CODEC_AAC           3
#define CODEC_MP3           4

/* MSM7227 MFC (Multi Format Codec) base address */
#define MSM7227_MFC_BASE    0xAA000000
#define MSM7227_MFC_SIZE    0x00100000

struct wave_codec {
    struct v4l2_device      v4l2_dev;
    struct video_device     *vdev;
    struct platform_device  *pdev;
    void __iomem            *mfc_base;
    int                     codec_type;
    int                     width;
    int                     height;
    int                     fps;
    int                     bitrate;
};

static struct wave_codec *codec_dev;

/* Supported formats */
static struct v4l2_fmtdesc wave_codec_formats[] = {
    {
        .index       = 0,
        .type        = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .description = "H.264",
        .pixelformat = V4L2_PIX_FMT_H264,
    },
    {
        .index       = 1,
        .type        = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .description = "MPEG4",
        .pixelformat = V4L2_PIX_FMT_MPEG4,
    },
    {
        .index       = 2,
        .type        = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .description = "H.263",
        .pixelformat = V4L2_PIX_FMT_H263,
    },
    {
        .index       = 3,
        .type        = V4L2_BUF_TYPE_VIDEO_OUTPUT,
        .description = "YUV 4:2:0",
        .pixelformat = V4L2_PIX_FMT_YUV420,
    },
};

static int wave_codec_enum_fmt(struct file *file, void *priv,
                               struct v4l2_fmtdesc *f)
{
    if (f->index >= ARRAY_SIZE(wave_codec_formats))
        return -EINVAL;

    *f = wave_codec_formats[f->index];
    return 0;
}

static int wave_codec_g_fmt(struct file *file, void *priv,
                            struct v4l2_format *f)
{
    struct wave_codec *codec = video_drvdata(file);

    f->fmt.pix.width        = codec->width;
    f->fmt.pix.height       = codec->height;
    f->fmt.pix.pixelformat  = V4L2_PIX_FMT_YUV420;
    f->fmt.pix.field        = V4L2_FIELD_NONE;
    f->fmt.pix.bytesperline = codec->width;
    f->fmt.pix.sizeimage    = codec->width * codec->height * 3 / 2;

    return 0;
}

static int wave_codec_s_fmt(struct file *file, void *priv,
                            struct v4l2_format *f)
{
    struct wave_codec *codec = video_drvdata(file);

    codec->width  = f->fmt.pix.width;
    codec->height = f->fmt.pix.height;

    return 0;
}

static int wave_codec_querycap(struct file *file, void *priv,
                               struct v4l2_capability *cap)
{
    strncpy(cap->driver, WAVE_CODEC_NAME, sizeof(cap->driver));
    strncpy(cap->card, "Wave S8500 Codec", sizeof(cap->card));
    cap->capabilities = V4L2_CAP_VIDEO_CAPTURE |
                        V4L2_CAP_VIDEO_OUTPUT  |
                        V4L2_CAP_STREAMING;
    return 0;
}

static const struct v4l2_ioctl_ops wave_codec_ioctl_ops = {
    .vidioc_querycap        = wave_codec_querycap,
    .vidioc_enum_fmt_vid_cap = wave_codec_enum_fmt,
    .vidioc_g_fmt_vid_cap   = wave_codec_g_fmt,
    .vidioc_s_fmt_vid_cap   = wave_codec_s_fmt,
};

static const struct v4l2_file_operations wave_codec_fops = {
    .owner          = THIS_MODULE,
    .unlocked_ioctl = video_ioctl2,
};

static int wave_codec_probe(struct platform_device *pdev)
{
    int ret;

    codec_dev = kzalloc(sizeof(*codec_dev), GFP_KERNEL);
    if (!codec_dev)
        return -ENOMEM;

    codec_dev->pdev   = pdev;
    codec_dev->width  = 480;
    codec_dev->height = 800;
    codec_dev->fps    = 30;
    codec_dev->bitrate = 2000000;

    /* Map MFC hardware */
    codec_dev->mfc_base = ioremap(MSM7227_MFC_BASE, MSM7227_MFC_SIZE);
    if (!codec_dev->mfc_base)
        dev_warn(&pdev->dev, "Could not map MFC registers\n");

    ret = v4l2_device_register(&pdev->dev, &codec_dev->v4l2_dev);
    if (ret) {
        kfree(codec_dev);
        return ret;
    }

    codec_dev->vdev = video_device_alloc();
    if (!codec_dev->vdev) {
        v4l2_device_unregister(&codec_dev->v4l2_dev);
        kfree(codec_dev);
        return -ENOMEM;
    }

    strncpy(codec_dev->vdev->name, WAVE_CODEC_NAME,
            sizeof(codec_dev->vdev->name));
    codec_dev->vdev->fops       = &wave_codec_fops;
    codec_dev->vdev->ioctl_ops  = &wave_codec_ioctl_ops;
    codec_dev->vdev->v4l2_dev   = &codec_dev->v4l2_dev;
    codec_dev->vdev->release    = video_device_release;
    video_set_drvdata(codec_dev->vdev, codec_dev);

    ret = video_register_device(codec_dev->vdev, VFL_TYPE_GRABBER, -1);
    if (ret) {
        video_device_release(codec_dev->vdev);
        v4l2_device_unregister(&codec_dev->v4l2_dev);
        kfree(codec_dev);
        return ret;
    }

    platform_set_drvdata(pdev, codec_dev);
    dev_info(&pdev->dev, "Wave S8500 codec initialized (H264/MPEG4/H263)\n");
    return 0;
}

static int wave_codec_remove(struct platform_device *pdev)
{
    video_unregister_device(codec_dev->vdev);
    v4l2_device_unregister(&codec_dev->v4l2_dev);
    if (codec_dev->mfc_base)
        iounmap(codec_dev->mfc_base);
    kfree(codec_dev);
    return 0;
}

static struct platform_driver wave_codec_driver = {
    .probe  = wave_codec_probe,
    .remove = wave_codec_remove,
    .driver = {
        .name  = WAVE_CODEC_NAME,
        .owner = THIS_MODULE,
    },
};

module_platform_driver(wave_codec_driver);

MODULE_AUTHOR("Bada OS Reconstruction Project");
MODULE_DESCRIPTION("Samsung Wave S8500 Video Codec Driver (H264/MPEG4)");
MODULE_LICENSE("GPL v2");