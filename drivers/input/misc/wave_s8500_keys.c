/*
 * Pie Kernel - Samsung Wave S8500 Physical Keys Driver
 * Power button, Volume Up, Volume Down
 *
 * Copyright (C) 2026 Bada OS Reconstruction Project
 * GPL v2
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/slab.h>

#define WAVE_KEYS_NAME       "wave_s8500_keys"

/* GPIO pins for Wave S8500 buttons */
#define GPIO_POWER_KEY       1
#define GPIO_VOL_UP          2
#define GPIO_VOL_DOWN        3

struct wave_key {
    int     gpio;
    int     irq;
    int     keycode;
    char    *name;
};

static struct wave_key wave_keys[] = {
    { GPIO_POWER_KEY, 0, KEY_POWER,      "power"      },
    { GPIO_VOL_UP,    0, KEY_VOLUMEUP,   "volume_up"  },
    { GPIO_VOL_DOWN,  0, KEY_VOLUMEDOWN, "volume_down" },
};

struct wave_keys_dev {
    struct input_dev *input;
};

static irqreturn_t wave_key_irq_handler(int irq, void *dev_id)
{
    struct wave_key *key = dev_id;
    struct wave_keys_dev *keys = input_get_drvdata(
        platform_get_drvdata(
            to_platform_device(
                &platform_bus)));

    int pressed = !gpio_get_value(key->gpio);

    input_report_key(keys->input, key->keycode, pressed);
    input_sync(keys->input);

    return IRQ_HANDLED;
}

static int wave_keys_probe(struct platform_device *pdev)
{
    struct wave_keys_dev *keys;
    int i, ret;

    keys = kzalloc(sizeof(*keys), GFP_KERNEL);
    if (!keys)
        return -ENOMEM;

    keys->input = input_allocate_device();
    if (!keys->input) {
        kfree(keys);
        return -ENOMEM;
    }

    keys->input->name = WAVE_KEYS_NAME;
    keys->input->id.bustype = BUS_HOST;

    set_bit(EV_KEY, keys->input->evbit);
    set_bit(KEY_POWER,      keys->input->keybit);
    set_bit(KEY_VOLUMEUP,   keys->input->keybit);
    set_bit(KEY_VOLUMEDOWN, keys->input->keybit);

    for (i = 0; i < ARRAY_SIZE(wave_keys); i++) {
        struct wave_key *key = &wave_keys[i];

        ret = gpio_request(key->gpio, key->name);
        if (ret) {
            dev_err(&pdev->dev, "Failed to request GPIO %d\n",
                    key->gpio);
            continue;
        }

        gpio_direction_input(key->gpio);
        key->irq = gpio_to_irq(key->gpio);

        ret = request_irq(key->irq, wave_key_irq_handler,
                         IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
                         key->name, key);
        if (ret) {
            gpio_free(key->gpio);
            dev_err(&pdev->dev, "Failed to request IRQ for %s\n",
                    key->name);
        }
    }

    input_set_drvdata(keys->input, keys);

    ret = input_register_device(keys->input);
    if (ret) {
        for (i = 0; i < ARRAY_SIZE(wave_keys); i++) {
            free_irq(wave_keys[i].irq, &wave_keys[i]);
            gpio_free(wave_keys[i].gpio);
        }
        input_free_device(keys->input);
        kfree(keys);
        return ret;
    }

    platform_set_drvdata(pdev, keys);
    dev_info(&pdev->dev, "Wave S8500 keys initialized\n");
    return 0;
}

static int wave_keys_remove(struct platform_device *pdev)
{
    struct wave_keys_dev *keys = platform_get_drvdata(pdev);
    int i;

    for (i = 0; i < ARRAY_SIZE(wave_keys); i++) {
        free_irq(wave_keys[i].irq, &wave_keys[i]);
        gpio_free(wave_keys[i].gpio);
    }

    input_unregister_device(keys->input);
    kfree(keys);
    return 0;
}

static struct platform_driver wave_keys_driver = {
    .probe  = wave_keys_probe,
    .remove = wave_keys_remove,
    .driver = {
        .name  = WAVE_KEYS_NAME,
        .owner = THIS_MODULE,
    },
};

module_platform_driver(wave_keys_driver);

MODULE_AUTHOR("Bada OS Reconstruction Project");
MODULE_DESCRIPTION("Samsung Wave S8500 Physical Keys Driver");
MODULE_LICENSE("GPL v2");