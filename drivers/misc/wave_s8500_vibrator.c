/*
 * Pie Kernel - Samsung Wave S8500 Vibrator Driver
 * Corresponds to Bada OS haptic feedback APIs
 *
 * Copyright (C) 2026 Bada OS Reconstruction Project
 * GPL v2
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/timer.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>

#define WAVE_VIB_NAME    "wave_s8500_vibrator"
#define VIB_DEFAULT_MS   200

struct wave_vib {
    struct cdev          cdev;
    struct platform_device *pdev;
    struct pwm_device    *pwm;
    struct timer_list    timer;
    int                  enabled;
};

static dev_t vib_devno;
static struct class *vib_class;
static struct wave_vib *vib_dev;

static void wave_vib_stop(struct timer_list *t)
{
    struct wave_vib *vib = vib_dev;

    if (vib->pwm)
        pwm_disable(vib->pwm);
    vib->enabled = 0;
}

static void wave_vib_start(struct wave_vib *vib, int duration_ms)
{
    if (vib->pwm) {
        pwm_config(vib->pwm, 500000, 1000000); /* 50% duty, 1kHz */
        pwm_enable(vib->pwm);
    }
    vib->enabled = 1;

    mod_timer(&vib->timer,
             jiffies + msecs_to_jiffies(duration_ms));
}

static ssize_t wave_vib_write(struct file *file, const char __user *buf,
                              size_t count, loff_t *ppos)
{
    struct wave_vib *vib = file->private_data;
    char kbuf[16];
    int duration_ms;

    if (count > sizeof(kbuf) - 1)
        count = sizeof(kbuf) - 1;

    if (copy_from_user(kbuf, buf, count))
        return -EFAULT;

    kbuf[count] = '\0';

    if (kstrtoint(kbuf, 10, &duration_ms))
        duration_ms = VIB_DEFAULT_MS;

    if (duration_ms > 0)
        wave_vib_start(vib, duration_ms);

    return count;
}

static int wave_vib_open(struct inode *inode, struct file *file)
{
    file->private_data = vib_dev;
    return 0;
}

static const struct file_operations wave_vib_fops = {
    .owner = THIS_MODULE,
    .open  = wave_vib_open,
    .write = wave_vib_write,
};

static int wave_vib_probe(struct platform_device *pdev)
{
    int ret;

    vib_dev = kzalloc(sizeof(*vib_dev), GFP_KERNEL);
    if (!vib_dev)
        return -ENOMEM;

    vib_dev->pdev = pdev;
    vib_dev->pwm = pwm_get(&pdev->dev, NULL);
    if (IS_ERR(vib_dev->pwm)) {
        dev_warn(&pdev->dev, "No PWM found, vibrator disabled\n");
        vib_dev->pwm = NULL;
    }

    timer_setup(&vib_dev->timer, wave_vib_stop, 0);

    ret = alloc_chrdev_region(&vib_devno, 0, 1, WAVE_VIB_NAME);
    if (ret < 0) {
        kfree(vib_dev);
        return ret;
    }

    cdev_init(&vib_dev->cdev, &wave_vib_fops);
    cdev_add(&vib_dev->cdev, vib_devno, 1);

    vib_class = class_create(THIS_MODULE, WAVE_VIB_NAME);
    device_create(vib_class, NULL, vib_devno, NULL, WAVE_VIB_NAME);

    platform_set_drvdata(pdev, vib_dev);
    dev_info(&pdev->dev, "Wave S8500 vibrator initialized\n");
    return 0;
}

static int wave_vib_remove(struct platform_device *pdev)
{
    del_timer_sync(&vib_dev->timer);
    if (vib_dev->pwm)
        pwm_put(vib_dev->pwm);
    device_destroy(vib_class, vib_devno);
    class_destroy(vib_class);
    cdev_del(&vib_dev->cdev);
    unregister_chrdev_region(vib_devno, 1);
    kfree(vib_dev);
    return 0;
}

static struct platform_driver wave_vib_driver = {
    .probe  = wave_vib_probe,
    .remove = wave_vib_remove,
    .driver = {
        .name  = WAVE_VIB_NAME,
        .owner = THIS_MODULE,
    },
};

module_platform_driver(wave_vib_driver);

MODULE_AUTHOR("Bada OS Reconstruction Project");
MODULE_DESCRIPTION("Samsung Wave S8500 Vibrator Driver");
MODULE_LICENSE("GPL v2");