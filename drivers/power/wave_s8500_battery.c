/*
 * Pie Kernel - Samsung Wave S8500 Battery Driver
 * Corresponds to Bada OS Osp::System battery APIs
 *
 * Copyright (C) 2026 Bada OS Reconstruction Project
 * GPL v2
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>

#define WAVE_BATT_NAME       "wave_s8500_battery"
#define WAVE_BATT_I2C_ADDR   0x36

#define BATT_REG_VOLTAGE     0x02
#define BATT_REG_CAPACITY    0x04
#define BATT_REG_STATUS      0x06

#define BATT_STATUS_CHARGING 0x01
#define BATT_STATUS_FULL     0x02

#define BATT_POLL_MS         30000

struct wave_battery {
    struct i2c_client       *client;
    struct power_supply     *psy;
    struct delayed_work     work;
    int                     voltage_uv;
    int                     capacity;
    int                     status;
};

static int batt_read_reg16(struct i2c_client *client, u8 reg)
{
    u8 buf[2];
    i2c_master_send(client, &reg, 1);
    i2c_master_recv(client, buf, 2);
    return (buf[1] << 8) | buf[0];
}

static void wave_battery_update(struct work_struct *work)
{
    struct wave_battery *batt =
        container_of(work, struct wave_battery, work.work);

    batt->voltage_uv = batt_read_reg16(batt->client, BATT_REG_VOLTAGE) * 1000;
    batt->capacity   = batt_read_reg16(batt->client, BATT_REG_CAPACITY);
    batt->status     = i2c_smbus_read_byte_data(batt->client,
                                                 BATT_REG_STATUS);

    power_supply_changed(batt->psy);
    schedule_delayed_work(&batt->work, msecs_to_jiffies(BATT_POLL_MS));
}

static int wave_battery_get_property(struct power_supply *psy,
                                     enum power_supply_property psp,
                                     union power_supply_propval *val)
{
    struct wave_battery *batt = power_supply_get_drvdata(psy);

    switch (psp) {
    case POWER_SUPPLY_PROP_STATUS:
        if (batt->status & BATT_STATUS_FULL)
            val->intval = POWER_SUPPLY_STATUS_FULL;
        else if (batt->status & BATT_STATUS_CHARGING)
            val->intval = POWER_SUPPLY_STATUS_CHARGING;
        else
            val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
        break;
    case POWER_SUPPLY_PROP_CAPACITY:
        val->intval = batt->capacity;
        break;
    case POWER_SUPPLY_PROP_VOLTAGE_NOW:
        val->intval = batt->voltage_uv;
        break;
    case POWER_SUPPLY_PROP_TECHNOLOGY:
        val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
        break;
    case POWER_SUPPLY_PROP_PRESENT:
        val->intval = 1;
        break;
    default:
        return -EINVAL;
    }
    return 0;
}

static enum power_supply_property wave_battery_props[] = {
    POWER_SUPPLY_PROP_STATUS,
    POWER_SUPPLY_PROP_CAPACITY,
    POWER_SUPPLY_PROP_VOLTAGE_NOW,
    POWER_SUPPLY_PROP_TECHNOLOGY,
    POWER_SUPPLY_PROP_PRESENT,
};

static const struct power_supply_desc wave_battery_desc = {
    .name           = WAVE_BATT_NAME,
    .type           = POWER_SUPPLY_TYPE_BATTERY,
    .properties     = wave_battery_props,
    .num_properties = ARRAY_SIZE(wave_battery_props),
    .get_property   = wave_battery_get_property,
};

static int wave_battery_probe(struct i2c_client *client,
                              const struct i2c_device_id *id)
{
    struct wave_battery *batt;
    struct power_supply_config psy_cfg = {};

    batt = kzalloc(sizeof(*batt), GFP_KERNEL);
    if (!batt)
        return -ENOMEM;

    batt->client = client;
    psy_cfg.drv_data = batt;

    batt->psy = power_supply_register(&client->dev, &wave_battery_desc,
                                      &psy_cfg);
    if (IS_ERR(batt->psy)) {
        kfree(batt);
        return PTR_ERR(batt->psy);
    }

    INIT_DELAYED_WORK(&batt->work, wave_battery_update);
    schedule_delayed_work(&batt->work, 0);

    i2c_set_clientdata(client, batt);
    dev_info(&client->dev, "Wave S8500 battery monitor initialized\n");
    return 0;
}

static int wave_battery_remove(struct i2c_client *client)
{
    struct wave_battery *batt = i2c_get_clientdata(client);
    cancel_delayed_work_sync(&batt->work);
    power_supply_unregister(batt->psy);
    kfree(batt);
    return 0;
}

static const struct i2c_device_id wave_battery_id[] = {
    { WAVE_BATT_NAME, 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, wave_battery_id);

static struct i2c_driver wave_battery_driver = {
    .driver = {
        .name  = WAVE_BATT_NAME,
        .owner = THIS_MODULE,
    },
    .probe    = wave_battery_probe,
    .remove   = wave_battery_remove,
    .id_table = wave_battery_id,
};

module_i2c_driver(wave_battery_driver);

MODULE_AUTHOR("Bada OS Reconstruction Project");
MODULE_DESCRIPTION("Samsung Wave S8500 Battery Driver");
MODULE_LICENSE("GPL v2");