/*
 * Pie Kernel - Samsung Wave S8500 Magnetometer (Compass) Driver
 * Corresponds to Bada OS msMagnetic.so service
 *
 * Copyright (C) 2026 Bada OS Reconstruction Project
 * GPL v2
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/delay.h>

#define WAVE_MAG_NAME       "wave_s8500_mag"
#define WAVE_MAG_I2C_ADDR   0x0E

#define MAG_REG_CTRL        0x00
#define MAG_REG_DATA_X      0x10
#define MAG_REG_DATA_Y      0x12
#define MAG_REG_DATA_Z      0x14

#define MAG_CTRL_ENABLE     0x01
#define MAG_POLL_MS         200

struct wave_mag {
    struct i2c_client    *client;
    struct input_dev     *input;
    struct delayed_work  work;
};

static int mag_read_axis(struct i2c_client *client, u8 reg)
{
    u8 buf[2];
    s16 val;

    i2c_master_send(client, &reg, 1);
    i2c_master_recv(client, buf, 2);

    val = (buf[1] << 8) | buf[0];
    return val;
}

static void wave_mag_poll(struct work_struct *work)
{
    struct wave_mag *mag =
        container_of(work, struct wave_mag, work.work);
    int x, y, z;

    x = mag_read_axis(mag->client, MAG_REG_DATA_X);
    y = mag_read_axis(mag->client, MAG_REG_DATA_Y);
    z = mag_read_axis(mag->client, MAG_REG_DATA_Z);

    input_report_abs(mag->input, ABS_X, x);
    input_report_abs(mag->input, ABS_Y, y);
    input_report_abs(mag->input, ABS_Z, z);
    input_sync(mag->input);

    schedule_delayed_work(&mag->work,
                          msecs_to_jiffies(MAG_POLL_MS));
}

static int wave_mag_probe(struct i2c_client *client,
                          const struct i2c_device_id *id)
{
    struct wave_mag *mag;
    int ret;

    mag = kzalloc(sizeof(*mag), GFP_KERNEL);
    if (!mag)
        return -ENOMEM;

    mag->client = client;
    mag->input = input_allocate_device();
    if (!mag->input) {
        kfree(mag);
        return -ENOMEM;
    }

    mag->input->name = WAVE_MAG_NAME;
    mag->input->id.bustype = BUS_I2C;

    set_bit(EV_ABS, mag->input->evbit);
    input_set_abs_params(mag->input, ABS_X, -8192, 8191, 0, 0);
    input_set_abs_params(mag->input, ABS_Y, -8192, 8191, 0, 0);
    input_set_abs_params(mag->input, ABS_Z, -8192, 8191, 0, 0);

    ret = input_register_device(mag->input);
    if (ret) {
        input_free_device(mag->input);
        kfree(mag);
        return ret;
    }

    i2c_smbus_write_byte_data(client, MAG_REG_CTRL, MAG_CTRL_ENABLE);

    INIT_DELAYED_WORK(&mag->work, wave_mag_poll);
    schedule_delayed_work(&mag->work, msecs_to_jiffies(MAG_POLL_MS));

    i2c_set_clientdata(client, mag);
    dev_info(&client->dev, "Wave S8500 magnetometer initialized\n");
    return 0;
}

static int wave_mag_remove(struct i2c_client *client)
{
    struct wave_mag *mag = i2c_get_clientdata(client);
    cancel_delayed_work_sync(&mag->work);
    input_unregister_device(mag->input);
    kfree(mag);
    return 0;
}

static const struct i2c_device_id wave_mag_id[] = {
    { WAVE_MAG_NAME, 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, wave_mag_id);

static struct i2c_driver wave_mag_driver = {
    .driver = {
        .name  = WAVE_MAG_NAME,
        .owner = THIS_MODULE,
    },
    .probe    = wave_mag_probe,
    .remove   = wave_mag_remove,
    .id_table = wave_mag_id,
};

module_i2c_driver(wave_mag_driver);

MODULE_AUTHOR("Bada OS Reconstruction Project");
MODULE_DESCRIPTION("Samsung Wave S8500 Magnetometer Driver");
MODULE_LICENSE("GPL v2");