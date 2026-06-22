/*
 * Pie Kernel - Samsung Wave S8500 Wakelock Driver
 * Screen on/off and wakelock management for Bada OS
 * Required by Bada OS Osp::System::PowerManager
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
#include <linux/wakelock.h>
#include <linux/input.h>

#define WAVE_WAKELOCK_NAME  "wave_s8500_wakelock"

/* Wakelock types matching Bada PowerManager */
#define WAKELOCK_SCREEN_ON      0
#define WAKELOCK_SCREEN_DIM     1
#define WAKELOCK_PARTIAL        2
#define WAKELOCK_SCREEN_BRIGHT  3

struct wave_wakelock {
    struct cdev         cdev;
    struct platform_device *pdev;
    struct wake_lock    screen_lock;
    struct wake_lock    partial_lock;
    struct input_dev    *input;
    int                 screen_state;
};

static dev_t wakelock_devno;
static struct class *wakelock_class;
static struct wave_wakelock *wakelock_dev;

static int wave_wakelock_open(struct inode *inode, struct file *file)
{
    file->private_data = wakelock_dev;
    return 0;
}

static ssize_t wave_wakelock_write(struct file *file,
                                   const char __user *buf,
                                   size_t count, loff_t *ppos)
{
    struct wave_wakelock *wl = file->private_data;
    char kbuf[32];
    int cmd;

    if (count > sizeof(kbuf) - 1)
        count = sizeof(kbuf) - 1;

    if (copy_from_user(kbuf, buf, count))
        return -EFAULT;

    kbuf[count] = '\0';

    if (kstrtoint(kbuf, 10, &cmd))
        return -EINVAL;

    switch (cmd) {
    case WAKELOCK_SCREEN_ON:
        /* Screen on — acquire screen wakelock */
        wake_lock(&wl->screen_lock);
        wl->screen_state = 1;

        /* Send power key event to turn on display */
        input_report_key(wl->input, KEY_POWER, 1);
        input_sync(wl->input);
        input_report_key(wl->input, KEY_POWER, 0);
        input_sync(wl->input);

        dev_info(&wl->pdev->dev, "Screen ON\n");
        break;

    case WAKELOCK_SCREEN_DIM:
        /* Screen dimmed */
        wl->screen_state = 2;
        dev_info(&wl->pdev->dev, "Screen DIM\n");
        break;

    case WAKELOCK_PARTIAL:
        /* Partial wakelock - CPU on, screen off */
        wake_lock(&wl->partial_lock);
        wake_unlock(&wl->screen_lock);
        wl->screen_state = 0;
        dev_info(&wl->pdev->dev, "Partial wakelock\n");
        break;

    case WAKELOCK_SCREEN_BRIGHT:
        /* Screen bright */
        wake_lock(&wl->screen_lock);
        wl->screen_state = 3;
        dev_info(&wl->pdev->dev, "Screen BRIGHT\n");
        break;

    default:
        /* Release all wakelocks */
        wake_unlock(&wl->screen_lock);
        wake_unlock(&wl->partial_lock);
        wl->screen_state = 0;
        break;
    }

    return count;
}

static ssize_t wave_wakelock_read(struct file *file, char __user *buf,
                                  size_t count, loff_t *ppos)
{
    struct wave_wakelock *wl = file->private_data;
    char kbuf[8];
    int len;

    len = snprintf(kbuf, sizeof(kbuf), "%d\n", wl->screen_state);

    if (copy_to_user(buf, kbuf, len))
        return -EFAULT;

    return len;
}

static const struct file_operations wave_wakelock_fops = {
    .owner = THIS_MODULE,
    .open  = wave_wakelock_open,
    .read  = wave_wakelock_read,
    .write = wave_wakelock_write,
};

static int wave_wakelock_probe(struct platform_device *pdev)
{
    int ret;

    wakelock_dev = kzalloc(sizeof(*wakelock_dev), GFP_KERNEL);
    if (!wakelock_dev)
        return -ENOMEM;

    wakelock_dev->pdev = pdev;

    /* Initialize wakelocks */
    wake_lock_init(&wakelock_dev->screen_lock,
                   WAKE_LOCK_SUSPEND, "screen");
    wake_lock_init(&wakelock_dev->partial_lock,
                   WAKE_LOCK_SUSPEND, "partial");

    /* Input device for screen power events */
    wakelock_dev->input = input_allocate_device();
    if (!wakelock_dev->input) {
        kfree(wakelock_dev);
        return -ENOMEM;
    }

    wakelock_dev->input->name = "wave_s8500_power";
    set_bit(EV_KEY, wakelock_dev->input->evbit);
    set_bit(KEY_POWER, wakelock_dev->input->keybit);

    ret = input_register_device(wakelock_dev->input);
    if (ret) {
        input_free_device(wakelock_dev->input);
        kfree(wakelock_dev);
        return ret;
    }

    ret = alloc_chrdev_region(&wakelock_devno, 0, 1, WAVE_WAKELOCK_NAME);
    if (ret < 0) {
        input_unregister_device(wakelock_dev->input);
        kfree(wakelock_dev);
        return ret;
    }

    cdev_init(&wakelock_dev->cdev, &wave_wakelock_fops);
    cdev_add(&wakelock_dev->cdev, wakelock_devno, 1);

    wakelock_class = class_create(THIS_MODULE, WAVE_WAKELOCK_NAME);
    device_create(wakelock_class, NULL, wakelock_devno, NULL,
                  WAVE_WAKELOCK_NAME);

    platform_set_drvdata(pdev, wakelock_dev);
    dev_info(&pdev->dev, "Wave S8500 wakelock initialized\n");
    return 0;
}

static int wave_wakelock_remove(struct platform_device *pdev)
{
    wake_lock_destroy(&wakelock_dev->screen_lock);
    wake_lock_destroy(&wakelock_dev->partial_lock);
    input_unregister_device(wakelock_dev->input);
    device_destroy(wakelock_class, wakelock_devno);
    class_destroy(wakelock_class);
    cdev_del(&wakelock_dev->cdev);
    unregister_chrdev_region(wakelock_devno, 1);
    kfree(wakelock_dev);
    return 0;
}

static struct platform_driver wave_wakelock_driver = {
    .probe  = wave_wakelock_probe,
    .remove = wave_wakelock_remove,
    .driver = {
        .name  = WAVE_WAKELOCK_NAME,
        .owner = THIS_MODULE,
    },
};

module_platform_driver(wave_wakelock_driver);

MODULE_AUTHOR("Bada OS Reconstruction Project");
MODULE_DESCRIPTION("Samsung Wave S8500 Wakelock Driver");
MODULE_LICENSE("GPL v2");