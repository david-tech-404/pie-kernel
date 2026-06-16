/*
 * Pie Kernel - Samsung Wave S8500 Touchscreen Driver
 * Samsung S8500 uses Synaptics touchscreen over I2C
 *
 * Copyright (C) 2024 Bada OS Reconstruction Project
 * GPL v2
 */

#include <linux/module.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/delay.h>

#define WAVE_TS_NAME        "wave_s8500_ts"
#define WAVE_TS_I2C_ADDR    0x20
#define WAVE_SCREEN_WIDTH   480
#define WAVE_SCREEN_HEIGHT  800

struct wave_ts {
    struct i2c_client   *client;
    struct input_dev    *input;
    int                 irq;
};

static irqreturn_t wave_ts_irq_handler(int irq, void *dev_id)
{
    struct wave_ts *ts = dev_id;
    u8 buf[6];
    int x, y, pressed;

    /* Read touch data from Synaptics controller */
    i2c_master_recv(ts->client, buf, sizeof(buf));

    pressed = buf[0] & 0x01;
    x = ((buf[1] & 0x0F) << 8) | buf[2];
    y = ((buf[1] & 0xF0) << 4) | buf[3];

    if (pressed) {
        input_report_abs(ts->input, ABS_X, x);
        input_report_abs(ts->input, ABS_Y, y);
        input_report_key(ts->input, BTN_TOUCH, 1);
    } else {
        input_report_key(ts->input, BTN_TOUCH, 0);
    }

    input_sync(ts->input);
    return IRQ_HANDLED;
}

static int wave_ts_probe(struct i2c_client *client,
                         const struct i2c_device_id *id)
{
    struct wave_ts *ts;
    struct input_dev *input;
    int ret;

    ts = kzalloc(sizeof(*ts), GFP_KERNEL);
    if (!ts)
        return -ENOMEM;

    input = input_allocate_device();
    if (!input) {
        kfree(ts);
        return -ENOMEM;
    }

    ts->client = client;
    ts->input  = input;
    ts->irq    = client->irq;

    input->name       = WAVE_TS_NAME;
    input->id.bustype = BUS_I2C;

    set_bit(EV_ABS, input->evbit);
    set_bit(EV_KEY, input->evbit);
    set_bit(BTN_TOUCH, input->keybit);

    input_set_abs_params(input, ABS_X, 0, WAVE_SCREEN_WIDTH, 0, 0);
    input_set_abs_params(input, ABS_Y, 0, WAVE_SCREEN_HEIGHT, 0, 0);

    ret = request_irq(ts->irq, wave_ts_irq_handler,
                      IRQF_TRIGGER_FALLING, WAVE_TS_NAME, ts);
    if (ret) {
        input_free_device(input);
        kfree(ts);
        return ret;
    }

    ret = input_register_device(input);
    if (ret) {
        free_irq(ts->irq, ts);
        input_free_device(input);
        kfree(ts);
        return ret;
    }

    i2c_set_clientdata(client, ts);
    dev_info(&client->dev, "Wave S8500 touchscreen initialized\n");
    return 0;
}

static int wave_ts_remove(struct i2c_client *client)
{
    struct wave_ts *ts = i2c_get_clientdata(client);
    free_irq(ts->irq, ts);
    input_unregister_device(ts->input);
    kfree(ts);
    return 0;
}

static const struct i2c_device_id wave_ts_id[] = {
    { WAVE_TS_NAME, 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, wave_ts_id);

static struct i2c_driver wave_ts_driver = {
    .driver = {
        .name = WAVE_TS_NAME,
        .owner = THIS_MODULE,
    },
    .probe    = wave_ts_probe,
    .remove   = wave_ts_remove,
    .id_table = wave_ts_id,
};

module_i2c_driver(wave_ts_driver);

MODULE_AUTHOR("Bada OS Reconstruction Project");
MODULE_DESCRIPTION("Samsung Wave S8500 Touchscreen Driver");
MODULE_LICENSE("GPL v2");