/*
 * Pie Kernel - Samsung Wave S8500 Proximity Sensor Driver
 * Corresponds to Bada OS msProximity.so service
 *
 * Copyright (C) 2026 Bada OS Reconstruction Project
 * GPL v2
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#define WAVE_PROX_NAME       "wave_s8500_proximity"
#define WAVE_PROX_I2C_ADDR   0x39

#define PROX_REG_CTRL        0x00
#define PROX_REG_STATUS      0x01
#define PROX_CTRL_ENABLE     0x01
#define PROX_NEAR_THRESHOLD  0x01

struct wave_prox {
    struct i2c_client    *client;
    struct input_dev     *input;
    int                  irq;
};

static irqreturn_t wave_prox_irq_handler(int irq, void *dev_id)
{
    struct wave_prox *prox = dev_id;
    u8 status;

    status = i2c_smbus_read_byte_data(prox->client, PROX_REG_STATUS);

    /* SW_FRONT_PROXIMITY: 1 = near, 0 = far */
    input_report_switch(prox->input, SW_FRONT_PROXIMITY,
                        status & PROX_NEAR_THRESHOLD);
    input_sync(prox->input);

    return IRQ_HANDLED;
}

static int wave_prox_probe(struct i2c_client *client,
                           const struct i2c_device_id *id)
{
    struct wave_prox *prox;
    int ret;

    prox = kzalloc(sizeof(*prox), GFP_KERNEL);
    if (!prox)
        return -ENOMEM;

    prox->client = client;
    prox->irq = client->irq;

    prox->input = input_allocate_device();
    if (!prox->input) {
        kfree(prox);
        return -ENOMEM;
    }

    prox->input->name = WAVE_PROX_NAME;
    prox->input->id.bustype = BUS_I2C;

    set_bit(EV_SW, prox->input->evbit);
    set_bit(SW_FRONT_PROXIMITY, prox->input->swbit);

    ret = request_irq(prox->irq, wave_prox_irq_handler,
                      IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
                      WAVE_PROX_NAME, prox);
    if (ret) {
        input_free_device(prox->input);
        kfree(prox);
        return ret;
    }

    ret = input_register_device(prox->input);
    if (ret) {
        free_irq(prox->irq, prox);
        input_free_device(prox->input);
        kfree(prox);
        return ret;
    }

    i2c_smbus_write_byte_data(client, PROX_REG_CTRL, PROX_CTRL_ENABLE);

    i2c_set_clientdata(client, prox);
    dev_info(&client->dev, "Wave S8500 proximity sensor initialized\n");
    return 0;
}

static int wave_prox_remove(struct i2c_client *client)
{
    struct wave_prox *prox = i2c_get_clientdata(client);
    free_irq(prox->irq, prox);
    input_unregister_device(prox->input);
    kfree(prox);
    return 0;
}

static const struct i2c_device_id wave_prox_id[] = {
    { WAVE_PROX_NAME, 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, wave_prox_id);

static struct i2c_driver wave_prox_driver = {
    .driver = {
        .name  = WAVE_PROX_NAME,
        .owner = THIS_MODULE,
    },
    .probe    = wave_prox_probe,
    .remove   = wave_prox_remove,
    .id_table = wave_prox_id,
};

module_i2c_driver(wave_prox_driver);

MODULE_AUTHOR("Bada OS Reconstruction Project");
MODULE_DESCRIPTION("Samsung Wave S8500 Proximity Sensor Driver");
MODULE_LICENSE("GPL v2");