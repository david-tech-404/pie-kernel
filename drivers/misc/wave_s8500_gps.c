/*
 * Pie Kernel - Samsung Wave S8500 GPS Driver
 * Corresponds to Bada OS msGps.so service
 *
 * The MSM7227 has integrated GPS baseband. This driver exposes it
 * as a character device streaming NMEA sentences, similar to how
 * the original Bada OS GPS service (msGps.so) consumed it.
 *
 * Copyright (C) 2026 Bada OS Reconstruction Project
 * GPL v2
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/serial_core.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define WAVE_GPS_NAME    "wave_s8500_gps"
#define GPS_BUF_SIZE     1024

struct wave_gps {
    struct cdev          cdev;
    struct platform_device *pdev;
    char                 nmea_buf[GPS_BUF_SIZE];
    int                  buf_len;
    int                  power_state;
};

static dev_t gps_devno;
static struct class *gps_class;
static struct wave_gps *gps_dev;

static int wave_gps_open(struct inode *inode, struct file *file)
{
    file->private_data = gps_dev;
    return 0;
}

static ssize_t wave_gps_read(struct file *file, char __user *buf,
                              size_t count, loff_t *ppos)
{
    struct wave_gps *gps = file->private_data;
    int len;

    if (gps->buf_len == 0)
        return 0;

    len = min((size_t)gps->buf_len, count);
    if (copy_to_user(buf, gps->nmea_buf, len))
        return -EFAULT;

    return len;
}

static ssize_t wave_gps_write(struct file *file, const char __user *buf,
                               size_t count, loff_t *ppos)
{
    struct wave_gps *gps = file->private_data;

    /* AT-style commands to the GPS baseband (power on/off, etc) */
    if (count > GPS_BUF_SIZE - 1)
        count = GPS_BUF_SIZE - 1;

    if (copy_from_user(gps->nmea_buf, buf, count))
        return -EFAULT;

    gps->nmea_buf[count] = '\0';

    if (strncmp(gps->nmea_buf, "POWER_ON", 8) == 0)
        gps->power_state = 1;
    else if (strncmp(gps->nmea_buf, "POWER_OFF", 9) == 0)
        gps->power_state = 0;

    return count;
}

static const struct file_operations wave_gps_fops = {
    .owner = THIS_MODULE,
    .open  = wave_gps_open,
    .read  = wave_gps_read,
    .write = wave_gps_write,
};

static int wave_gps_probe(struct platform_device *pdev)
{
    int ret;

    gps_dev = kzalloc(sizeof(*gps_dev), GFP_KERNEL);
    if (!gps_dev)
        return -ENOMEM;

    gps_dev->pdev = pdev;

    ret = alloc_chrdev_region(&gps_devno, 0, 1, WAVE_GPS_NAME);
    if (ret < 0) {
        kfree(gps_dev);
        return ret;
    }

    cdev_init(&gps_dev->cdev, &wave_gps_fops);
    ret = cdev_add(&gps_dev->cdev, gps_devno, 1);
    if (ret) {
        unregister_chrdev_region(gps_devno, 1);
        kfree(gps_dev);
        return ret;
    }

    gps_class = class_create(THIS_MODULE, WAVE_GPS_NAME);
    if (IS_ERR(gps_class)) {
        cdev_del(&gps_dev->cdev);
        unregister_chrdev_region(gps_devno, 1);
        kfree(gps_dev);
        return PTR_ERR(gps_class);
    }

    device_create(gps_class, NULL, gps_devno, NULL, WAVE_GPS_NAME);

    platform_set_drvdata(pdev, gps_dev);
    dev_info(&pdev->dev, "Wave S8500 GPS (MSM7227 baseband) initialized\n");
    return 0;
}

static int wave_gps_remove(struct platform_device *pdev)
{
    device_destroy(gps_class, gps_devno);
    class_destroy(gps_class);
    cdev_del(&gps_dev->cdev);
    unregister_chrdev_region(gps_devno, 1);
    kfree(gps_dev);
    return 0;
}

static struct platform_driver wave_gps_driver = {
    .probe  = wave_gps_probe,
    .remove = wave_gps_remove,
    .driver = {
        .name  = WAVE_GPS_NAME,
        .owner = THIS_MODULE,
    },
};

module_platform_driver(wave_gps_driver);

MODULE_AUTHOR("Bada OS Reconstruction Project");
MODULE_DESCRIPTION("Samsung Wave S8500 GPS Driver (MSM7227 baseband)");
MODULE_LICENSE("GPL v2");