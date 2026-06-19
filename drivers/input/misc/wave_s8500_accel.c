/*
 * Pie Kernel - Samsung Wave S8500 Accelerometer Driver
 * Corresponds to Bada OS msAccel.so service
 *
 * Copyright (C) 2026 Bada OS Reconstruction Project
 * GPL v2
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/interrupt.h>

#define WAVE_ACCEL_NAME      "wave_s8500_accel"
#define WAVE_ACCEL_I2C_ADDR  0x38

/* Accelerometer registers (generic 3-axis layout) */
#define ACCEL_REG_CTRL       0x00
#define ACCEL_REG_DATA_X     0x01
#define ACCEL_REG_DATA_Y     0x03
#define ACCEL_REG_DATA_Z     0x05

#define ACCEL_CTRL_ENABLE    0x01
#define ACCEL_POLL_MS        100

struct wave_accel {
    struct i2c_client    *client;
    struct input_dev     *input;
    struct delayed_work  work;
};

static int accel_read_axis(struct i2c_client *client, u8 reg)
{
    u8 buf[2];
    s16 val;

    i2c_master_send(client, &reg, 1);
    i2c_master_recv(client, buf, 2);

    val = (buf[1] << 8) | buf[0];
    return val;
}

static void wave_accel_poll(struct work_struct *work)
{
    struct wave_accel *accel =
        container_of(work, struct wave_accel, work.work);
    int x, y, z;

    x = accel_read_axis(accel->client, ACCEL_REG_DATA_X);
    y = accel_read_axis(accel->client, ACCEL_REG_DATA_Y);
    z = accel_read_axis(accel->client, ACCEL_REG_DATA_Z);

    input_report_abs(accel->input, ABS_X, x);
    input_report_abs(accel->input, ABS_Y, y);
    input_report_abs(accel->input, ABS_Z, z);
    input_sync(accel->input);

    schedule_delayed_work(&accel->work,
                          msecs_to_jiffies(ACCEL_POLL_MS));
}

static int wave_accel_probe(struct i2c_client *client,
                            const struct i2c_device_id *id)
{
    struct wave_accel *accel;
    int ret;

    accel = kzalloc(sizeof(*accel), GFP_KERNEL);
    if (!accel)
        return -ENOMEM;

    accel->client = client;
    accel->input = input_allocate_device();
    if (!accel->input) {
        kfree(accel);
        return -ENOMEM;
    }

    accel->input->name = WAVE_ACCEL_NAME;
    accel->input->id.bustype = BUS_I2C;

    set_bit(EV_ABS, accel->input->evbit);
    input_set_abs_params(accel->input, ABS_X, -32768, 32767, 0, 0);
    input_set_abs_params(accel->input, ABS_Y, -32768, 32767, 0, 0);
    input_set_abs_params(accel->input, ABS_Z, -32768, 32767, 0, 0);

    ret = input_register_device(accel->input);
    if (ret) {
        input_free_device(accel->input);
        kfree(accel);
        return ret;
    }

    /* Enable the sensor */
    i2c_smbus_write_byte_data(client, ACCEL_REG_CTRL, ACCEL_CTRL_ENABLE);

    INIT_DELAYED_WORK(&accel->work, wave_accel_poll);
    schedule_delayed_work(&accel->work, msecs_to_jiffies(ACCEL_POLL_MS));

    i2c_set_clientdata(client, accel);
    dev_info(&client->dev, "Wave S8500 accelerometer initialized\n");
    return 0;
}

static int wave_accel_remove(struct i2c_client *client)
{
    struct wave_accel *accel = i2c_get_clientdata(client);
    cancel_delayed_work_sync(&accel->work);
    input_unregister_device(accel->input);
    kfree(accel);
    return 0;
}

static const struct i2c_device_id wave_accel_id[] = {
    { WAVE_ACCEL_NAME, 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, wave_accel_id);

static struct i2c_driver wave_accel_driver = {
    .driver = {
        .name  = WAVE_ACCEL_NAME,
        .owner = THIS_MODULE,
    },
    .probe    = wave_accel_probe,
    .remove   = wave_accel_remove,
    .id_table = wave_accel_id,
};

module_i2c_driver(wave_accel_driver);

MODULE_AUTHOR("Bada OS Reconstruction Project");
MODULE_DESCRIPTION("Samsung Wave S8500 Accelerometer Driver");
MODULE_LICENSE("GPL v2");