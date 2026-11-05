// File operations (open, read, write, release)

#include "at24c256.h"

static int my_i2c_open(struct inode *inode, struct file *file)
{
    struct my_i2c_dev *dev;
    dev = container_of(inode->i_cdev, struct my_i2c_dev, cdev);
    file->private_data = dev;
    return 0;
}

static ssize_t my_i2c_read(struct file *file, char __user *buf, size_t len, loff_t* off)
{
    struct my_i2c_dev *dev = file->private_data;
    ssize_t ret;

    if (*off >= EEPROM_SIZE)
        return 0;

    if (*off + len > EEPROM_SIZE)
        len = EEPROM_SIZE - *off;

    if (len > BUFFER_SIZE)
        len = BUFFER_SIZE;

    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

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

static loff_t my_i2c_llseek(struct file *file, loff_t offset, int whence)
{
    return fixed_size_llseek(file, offset, whence, AT24C256_TOTAL_SIZE);
}

static ssize_t my_i2c_write(struct file *file, const char __user *buf, size_t len, loff_t* off)
{
    struct my_i2c_dev *dev = file->private_data;
    ssize_t ret;

    pr_info("my_i2c_write: Entered with len=%zu, off=%lld\n", len, *off);

    if (*off >= EEPROM_SIZE) {
        pr_err("my_i2c_write: Offset %lld exceeds EEPROM size %d (-ENOSPC)\n", *off, EEPROM_SIZE);
        return -ENOSPC;
    }

    if (len > (BUFFER_SIZE - 2)) {
        pr_info("my_i2c_write: Capping requested len %zu to max allowed %d\n", len, BUFFER_SIZE - 2);
        len = BUFFER_SIZE - 2;
    }

    pr_info("my_i2c_write: Attempting to acquire mutex lock...\n");
    if (mutex_lock_interruptible(&dev->lock)) {
        pr_err("my_i2c_write: Mutex lock interrupted by signal (-ERESTARTSYS)\n");
        return -ERESTARTSYS;
    }
    pr_info("my_i2c_write: Mutex lock acquired\n");

    if (copy_from_user(dev->buf, buf, len)) {
        pr_err("my_i2c_write: copy_from_user failed (-EFAULT)\n");
        mutex_unlock(&dev->lock);
        pr_info("my_i2c_write: Mutex unlocked after copy_from_user failure\n");
        return -EFAULT;
    }
    pr_info("my_i2c_write: Successfully copied %zu bytes from user space\n", len);

    pr_info("my_i2c_write: Calling at24c256_hw_write (len=%zu, off=%lld)\n", len, *off);
    ret = at24c256_hw_write(dev, dev->buf, len, *off);
    if (ret < 0) {
        pr_err("my_i2c_write: at24c256_hw_write failed with error %zd\n", ret);
        mutex_unlock(&dev->lock);
        pr_info("my_i2c_write: Mutex unlocked after hardware write failure\n");
        return ret;
    }
    pr_info("my_i2c_write: at24c256_hw_write succeeded, wrote %zd bytes\n", ret);

    *off += ret;
    pr_info("my_i2c_write: Updated offset to %lld\n", *off);

    mutex_unlock(&dev->lock);
    pr_info("my_i2c_write: Mutex unlocked. Returning %zd\n", ret);

    return ret;
}

static int my_i2c_release(struct inode *inode, struct file *file)
{
    return 0;
}

static int my_i2c_fsync(struct file *file, loff_t start, loff_t end, int datasync)
{
    struct my_i2c_dev *dev = file->private_data;
    int ret;

    if (!dev || !dev->client)
        return -ENODEV;

    /*
     * Serialize fsync with ongoing read/write operations.
     */
    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    /*
     * Check whether EEPROM is ready.
     */
    ret = at24c256_ack_poll(dev->client);

    if (ret < 0) {
        dev_err(&dev->client->dev, "fsync: EEPROM synchronization failed: %d\n", ret);
    } else {
        dev_dbg(&dev->client->dev, "fsync: EEPROM is ready\n");
    }

    mutex_unlock(&dev->lock);

    return ret;
}

const struct file_operations my_i2c_fops = {
    .owner   = THIS_MODULE,
    .open    = my_i2c_open,
    .read    = my_i2c_read,
    .write   = my_i2c_write,
    .llseek  = my_i2c_llseek,
    .release = my_i2c_release,
    .fsync = my_i2c_fsync,
};