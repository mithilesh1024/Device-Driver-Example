// File operations (open, read, write, release)

#include "at24c256.h"

static int my_i2c_open(struct inode *inode, struct file *file)
{
    struct my_i2c_dev *dev;
    dev = container_of(inode->i_cdev, struct my_i2c_dev, cdev);
    file->private_data = dev;
    return 0;
}

static ssize_t my_i2c_read(struct file *file, char __user *buf, size_t len, loff_t *off)
{
    struct my_i2c_dev *dev = file->private_data;
    ssize_t ret;

    if (*off >= EEPROM_SIZE)
        return 0;

    if (*off + len > EEPROM_SIZE)
        len = EEPROM_SIZE - *off;

    if (len > BUFFER_SIZE)
        len = BUFFER_SIZE;

    mutex_lock(&dev->lock);

    ret = at24c256_hw_read(dev, dev->buf, len, *off);
    if (ret < 0) {
        mutex_unlock(&dev->lock);
        return ret;
    }

    if (copy_to_user(buf, dev->buf, ret)) {
        mutex_unlock(&dev->lock);
        return -EFAULT;
    }

    *off += ret;
    mutex_unlock(&dev->lock);

    return ret;
}

static ssize_t my_i2c_write(struct file *file, const char __user *buf, size_t len, loff_t *off)
{
    struct my_i2c_dev *dev = file->private_data;
    ssize_t ret;

    if (*off >= EEPROM_SIZE)
        return -ENOSPC;

    if (len > (BUFFER_SIZE - 2))
        len = BUFFER_SIZE - 2;

    mutex_lock(&dev->lock);

    if (copy_from_user(dev->buf, buf, len)) {
        mutex_unlock(&dev->lock);
        return -EFAULT;
    }

    ret = at24c256_hw_write(dev, dev->buf, len, *off);
    if (ret < 0) {
        mutex_unlock(&dev->lock);
        return ret;
    }

    *off += ret;
    mutex_unlock(&dev->lock);

    return ret;
}

static int my_i2c_release(struct inode *inode, struct file *file)
{
    return 0;
}

const struct file_operations my_i2c_fops = {
    .owner   = THIS_MODULE,
    .open    = my_i2c_open,
    .read    = my_i2c_read,
    .write   = my_i2c_write,
    .llseek  = default_llseek,
    .release = my_i2c_release,
};