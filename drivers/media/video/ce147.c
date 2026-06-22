/*
 * Pie Kernel - Samsung CE147 Camera Driver
 * Samsung Wave S8500 uses CE147 5MP camera sensor
 *
 * Copyright (C) 2024 Bada OS Reconstruction Project
 * GPL v2
 *
 * Firmware: CE147_F3_0x05.bin (extracted from Bada OS 1.2)
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#define CE147_NAME          "ce147"
#define CE147_I2C_ADDR      0x78
#define CE147_FIRMWARE      "CE147_F3_0x05.bin"

/* CE147 registers */
#define CE147_REG_RESET     0x00
#define CE147_REG_MODE      0x01
#define CE147_REG_WIDTH     0x10
#define CE147_REG_HEIGHT    0x11
#define CE147_REG_FMT       0x12

/* Camera modes */
#define CE147_MODE_PREVIEW  0x01
#define CE147_MODE_CAPTURE  0x02
#define CE147_MODE_RECORD   0x03

/* Resolutions */
#define CE147_WIDTH_5MP     2560
#define CE147_HEIGHT_5MP    1920
#define CE147_WIDTH_VGA     640
#define CE147_HEIGHT_VGA    480

struct ce147_state {
    struct v4l2_subdev      sd;
    struct i2c_client       *client;
    const struct firmware   *fw;
    int                     mode;
    int                     width;
    int                     height;
};

static int ce147_write(struct i2c_client *client, u8 reg, u8 val)
{
    u8 buf[2] = { reg, val };
    return i2c_master_send(client, buf, 2);
}

static int ce147_read(struct i2c_client *client, u8 reg)
{
    u8 val;
    i2c_master_send(client, &reg, 1);
    i2c_master_recv(client, &val, 1);
    return val;
}

static int ce147_load_firmware(struct ce147_state *state)
{
    struct device *dev = &state->client->dev;
    int ret;

    ret = request_firmware(&state->fw, CE147_FIRMWARE, dev);
    if (ret) {
        dev_err(dev, "Failed to load CE147 firmware: %s\n",
                CE147_FIRMWARE);
        return ret;
    }

    dev_info(dev, "CE147 firmware loaded: %zu bytes\n",
             state->fw->size);
    return 0;
}

static int ce147_reset(struct ce147_state *state)
{
    ce147_write(state->client, CE147_REG_RESET, 0x01);
    msleep(10);
    ce147_write(state->client, CE147_REG_RESET, 0x00);
    msleep(50);
    return 0;
}

static int ce147_set_mode(struct ce147_state *state, int mode)
{
    state->mode = mode;
    return ce147_write(state->client, CE147_REG_MODE, mode);
}

/* V4L2 subdev ops */
static int ce147_s_stream(struct v4l2_subdev *sd, int enable)
{
    struct ce147_state *state =
        container_of(sd, struct ce147_state, sd);

    if (enable)
        return ce147_set_mode(state, CE147_MODE_PREVIEW);
    else
        return ce147_write(state->client, CE147_REG_MODE, 0x00);
}

static int ce147_enum_fmt(struct v4l2_subdev *sd,
                          struct v4l2_fmtdesc *f)
{
    static const struct v4l2_fmtdesc formats[] = {
        {
            .index       = 0,
            .type        = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            .description = "JPEG",
            .pixelformat = V4L2_PIX_FMT_JPEG,
        },
        {
            .index       = 1,
            .type        = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            .description = "YUV 4:2:2",
            .pixelformat = V4L2_PIX_FMT_YUYV,
        },
    };

    if (f->index >= ARRAY_SIZE(formats))
        return -EINVAL;

    *f = formats[f->index];
    return 0;
}

static int ce147_s_fmt(struct v4l2_subdev *sd,
                       struct v4l2_mbus_framefmt *fmt)
{
    struct ce147_state *state =
        container_of(sd, struct ce147_state, sd);

    state->width  = fmt->width;
    state->height = fmt->height;

    /* Set resolution on hardware */
    ce147_write(state->client, CE147_REG_WIDTH,  fmt->width >> 8);
    ce147_write(state->client, CE147_REG_HEIGHT, fmt->height >> 8);

    return 0;
}

static const struct v4l2_subdev_video_ops 
/* CE147 additional registers */
#define CE147_REG_FLASH     0x20
#define CE147_REG_ZOOM      0x21
#define CE147_REG_FOCUS     0x22
#define CE147_REG_FOCUS_STATUS 0x23
#define CE147_REG_WB        0x24
#define CE147_REG_EXPOSURE  0x25

/* Flash modes */
#define CE147_FLASH_OFF     0x00
#define CE147_FLASH_ON      0x01
#define CE147_FLASH_AUTO    0x02
#define CE147_FLASH_TORCH   0x03

/* Focus modes */
#define CE147_FOCUS_AUTO    0x00
#define CE147_FOCUS_MACRO   0x01
#define CE147_FOCUS_FIXED   0x02

static int ce147_s_ctrl(struct v4l2_subdev *sd,
                        struct v4l2_control *ctrl)
{
    struct ce147_state *state =
        container_of(sd, struct ce147_state, sd);

    switch (ctrl->id) {
    case V4L2_CID_FLASH_LED_MODE:
        switch (ctrl->value) {
        case V4L2_FLASH_LED_MODE_NONE:
            ce147_write(state->client, CE147_REG_FLASH,
                       CE147_FLASH_OFF);
            break;
        case V4L2_FLASH_LED_MODE_FLASH:
            ce147_write(state->client, CE147_REG_FLASH,
                       CE147_FLASH_ON);
            break;
        case V4L2_FLASH_LED_MODE_TORCH:
            ce147_write(state->client, CE147_REG_FLASH,
                       CE147_FLASH_TORCH);
            break;
        }
        break;

    case V4L2_CID_ZOOM_ABSOLUTE:
        /* Zoom range 0-30 */
        if (ctrl->value < 0 || ctrl->value > 30)
            return -EINVAL;
        ce147_write(state->client, CE147_REG_ZOOM, ctrl->value);
        break;

    case V4L2_CID_FOCUS_AUTO:
        if (ctrl->value)
            ce147_write(state->client, CE147_REG_FOCUS,
                       CE147_FOCUS_AUTO);
        else
            ce147_write(state->client, CE147_REG_FOCUS,
                       CE147_FOCUS_FIXED);
        break;

    case V4L2_CID_AUTO_FOCUS_START:
        /* Trigger autofocus */
        ce147_write(state->client, CE147_REG_FOCUS,
                   CE147_FOCUS_AUTO);
        msleep(100);
        break;

    case V4L2_CID_AUTO_WHITE_BALANCE:
        ce147_write(state->client, CE147_REG_WB, ctrl->value);
        break;

    case V4L2_CID_EXPOSURE:
        ce147_write(state->client, CE147_REG_EXPOSURE,
                   ctrl->value & 0xFF);
        break;

    default:
        return -EINVAL;
    }
    return 0;
}

static int ce147_g_ctrl(struct v4l2_subdev *sd,
                        struct v4l2_control *ctrl)
{
    struct ce147_state *state =
        container_of(sd, struct ce147_state, sd);

    switch (ctrl->id) {
    case V4L2_CID_AUTO_FOCUS_STATUS:
        ctrl->value = ce147_read(state->client,
                                 CE147_REG_FOCUS_STATUS);
        break;
    case V4L2_CID_ZOOM_ABSOLUTE:
        ctrl->value = ce147_read(state->client, CE147_REG_ZOOM);
        break;
    default:
        return -EINVAL;
    }
    return 0;
}

static const struct v4l2_subdev_core_ops ce147_core_ops = {
    .s_ctrl = ce147_s_ctrl,
    .g_ctrl = ce147_g_ctrl,
};

static const struct v4l2_subdev_video_ops ce147_video_ops = {
    .s_stream      = ce147_s_stream,
    .enum_mbus_fmt = ce147_enum_fmt,
    .s_mbus_fmt    = ce147_s_fmt,
};

static const struct v4l2_subdev_ops ce147_ops = {
    .core  = &ce147_core_ops,
    .video = &ce147_video_ops,
};

static const struct v4l2_subdev_video_ops ce147_video_ops = {
    .s_stream = ce147_s_stream,
};

static const struct v4l2_subdev_ops ce147_ops = {
    .video = &ce147_video_ops,
};

static int ce147_probe(struct i2c_client *client,
                       const struct i2c_device_id *id)
{
    struct ce147_state *state;
    int ret;

    state = kzalloc(sizeof(*state), GFP_KERNEL);
    if (!state)
        return -ENOMEM;

    state->client = client;
    state->width  = CE147_WIDTH_VGA;
    state->height = CE147_HEIGHT_VGA;

    v4l2_i2c_subdev_init(&state->sd, client, &ce147_ops);

    ret = ce147_load_firmware(state);
    if (ret) {
        kfree(state);
        return ret;
    }

    ret = ce147_reset(state);
    if (ret) {
        release_firmware(state->fw);
        kfree(state);
        return ret;
    }

    i2c_set_clientdata(client, state);
    dev_info(&client->dev, "CE147 5MP camera initialized\n");
    return 0;
}

static int ce147_remove(struct i2c_client *client)
{
    struct ce147_state *state = i2c_get_clientdata(client);
    release_firmware(state->fw);
    kfree(state);
    return 0;
}

static const struct i2c_device_id ce147_id[] = {
    { CE147_NAME, 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, ce147_id);

static struct i2c_driver ce147_driver = {
    .driver = {
        .name  = CE147_NAME,
        .owner = THIS_MODULE,
    },
    .probe    = ce147_probe,
    .remove   = ce147_remove,
    .id_table = ce147_id,
};

module_i2c_driver(ce147_driver);

MODULE_AUTHOR("Bada OS Reconstruction Project");
MODULE_DESCRIPTION("Samsung CE147 5MP Camera Driver");
MODULE_LICENSE("GPL v2");
MODULE_FIRMWARE(CE147_FIRMWARE);