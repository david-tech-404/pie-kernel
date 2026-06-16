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