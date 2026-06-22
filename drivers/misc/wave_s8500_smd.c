/*
 * Pie Kernel - Samsung Wave S8500 SMD Driver
 * Shared Memory Driver - communication between Linux and Qualcomm AMSS modem
 * Required by msvcConnectionManagerServer.so via /dev/smd0
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
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/io.h>

#define WAVE_SMD_NAME       "smd"
#define SMD_NUM_CHANNELS    4
#define SMD_BUF_SIZE        8192

/* SMD shared memory base address for MSM7227 */
#define MSM7227_SMD_BASE    0x00100000
#define MSM7227_SMD_SIZE    0x00020000

struct smd_channel {
    char        rx_buf[SMD_BUF_SIZE];
    int         rx_head;
    int         rx_tail;
    char        tx_buf[SMD_BUF_SIZE];
    int         tx_head;
    int         tx_tail;
    wait_queue_head_t rx_wait;
    spinlock_t  lock;
    int         open;
};

struct wave_smd {
    struct cdev         cdev;
    dev_t               devno;
    struct smd_channel  channels[SMD_NUM_CHANNELS];
    void __iomem        *smd_base;
};

static struct class *smd_class;
static struct wave_smd *smd_dev;

static int wave_smd_open(struct inode *inode, struct file *file)
{
    int minor = iminor(inode);
    struct smd_channel *ch;

    if (minor >= SMD_NUM_CHANNELS)
        return -ENODEV;

    ch = &smd_dev->channels[minor];

    spin_lock(&ch->lock);
    if (ch->open) {
        spin_unlock(&ch->lock);
        return -EBUSY;
    }
    ch->open = 1;
    spin_unlock(&ch->lock);

    file->private_data = ch;
    return 0;
}

static int wave_smd_release(struct inode *inode, struct file *file)
{
    struct smd_channel *ch = file->private_data;
    spin_lock(&ch->lock);
    ch->open = 0;
    spin_unlock(&ch->lock);
    return 0;
}

static ssize_t wave_smd_read(struct file *file, char __user *buf,
                              size_t count, loff_t *ppos)
{
    struct smd_channel *ch = file->private_data;
    int avail, len;

    if (wait_event_interruptible(ch->rx_wait,
        ch->rx_head != ch->rx_tail))
        return -ERESTARTSYS;

    spin_lock(&ch->lock);
    avail = (ch->rx_head - ch->rx_tail + SMD_BUF_SIZE) % SMD_BUF_SIZE;
    len = min((int)count, avail);

    if (copy_to_user(buf, ch->rx_buf + ch->rx_tail, len)) {
        spin_unlock(&ch->lock);
        return -EFAULT;
    }
    ch->rx_tail = (ch->rx_tail + len) % SMD_BUF_SIZE;
    spin_unlock(&ch->lock);

    return len;
}

static ssize_t wave_smd_write(struct file *file, const char __user *buf,
                               size_t count, loff_t *ppos)
{
    struct smd_channel *ch = file->private_data;
    char kbuf[SMD_BUF_SIZE];
    int i;

    if (count > SMD_BUF_SIZE - 1)
        count = SMD_BUF_SIZE - 1;

    if (copy_from_user(kbuf, buf, count))
        return -EFAULT;

    /* Echo back to rx for testing — real SMD would forward to modem */
    spin_lock(&ch->lock);
    for (i = 0; i < count; i++) {
        ch->rx_buf[ch->rx_head] = kbuf[i];
        ch->rx_head = (ch->rx_head + 1) % SMD_BUF_SIZE;
    }
    spin_unlock(&ch->lock);
    wake_up_interruptible(&ch->rx_wait);

    return count;
}

static unsigned int wave_smd_poll(struct file *file, poll_table *wait)
{
    struct smd_channel *ch = file->private_data;
    unsigned int mask = 0;

    poll_wait(file, &ch->rx_wait, wait);

    if (ch->rx_head != ch->rx_tail)
        mask |= POLLIN | POLLRDNORM;

    mask |= POLLOUT | POLLWRNORM;
    return mask;
}

static const struct file_operations wave_smd_fops = {
    .owner   = THIS_MODULE,
    .open    = wave_smd_open,
    .release = wave_smd_release,
    .read    = wave_smd_read,
    .write   = wave_smd_write,
    .poll    = wave_smd_poll,
};

static int wave_smd_probe(struct platform_device *pdev)
{
    int ret, i;

    smd_dev = kzalloc(sizeof(*smd_dev), GFP_KERNEL);
    if (!smd_dev)
        return -ENOMEM;

    smd_dev->smd_base = ioremap(MSM7227_SMD_BASE, MSM7227_SMD_SIZE);
    if (!smd_dev->smd_base) {
        dev_warn(&pdev->dev, "Could not map SMD memory\n");
    }

    for (i = 0; i < SMD_NUM_CHANNELS; i++) {
        init_waitqueue_head(&smd_dev->channels[i].rx_wait);
        spin_lock_init(&smd_dev->channels[i].lock);
    }

    ret = alloc_chrdev_region(&smd_dev->devno, 0, SMD_NUM_CHANNELS,
                              WAVE_SMD_NAME);
    if (ret < 0) {
        kfree(smd_dev);
        return ret;
    }

    cdev_init(&smd_dev->cdev, &wave_smd_fops);
    ret = cdev_add(&smd_dev->cdev, smd_dev->devno, SMD_NUM_CHANNELS);
    if (ret) {
        unregister_chrdev_region(smd_dev->devno, SMD_NUM_CHANNELS);
        kfree(smd_dev);
        return ret;
    }

    smd_class = class_create(THIS_MODULE, WAVE_SMD_NAME);
    for (i = 0; i < SMD_NUM_CHANNELS; i++) {
        device_create(smd_class, NULL,
                     MKDEV(MAJOR(smd_dev->devno), i),
                     NULL, "smd%d", i);
    }

    platform_set_drvdata(pdev, smd_dev);
    dev_info(&pdev->dev, "Wave S8500 SMD initialized (%d channels)\n",
             SMD_NUM_CHANNELS);
    return 0;
}

static int wave_smd_remove(struct platform_device *pdev)
{
    int i;
    for (i = 0; i < SMD_NUM_CHANNELS; i++)
        device_destroy(smd_class, MKDEV(MAJOR(smd_dev->devno), i));
    class_destroy(smd_class);
    cdev_del(&smd_dev->cdev);
    unregister_chrdev_region(smd_dev->devno, SMD_NUM_CHANNELS);
    if (smd_dev->smd_base)
        iounmap(smd_dev->smd_base);
    kfree(smd_dev);
    return 0;
}

static struct platform_driver wave_smd_driver = {
    .probe  = wave_smd_probe,
    .remove = wave_smd_remove,
    .driver = {
        .name  = "wave_s8500_smd",
        .owner = THIS_MODULE,
    },
};

module_platform_driver(wave_smd_driver);

MODULE_AUTHOR("Bada OS Reconstruction Project");
MODULE_DESCRIPTION("Samsung Wave S8500 SMD Driver");
MODULE_LICENSE("GPL v2");