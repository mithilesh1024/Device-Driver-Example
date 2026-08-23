// Common data structures and function prototypes

#ifndef AT24C256_H
#define AT24C256_H

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/mod_devicetable.h>

#define DRIVER_NAME "my_i2c_driver"
#define CLASS_NAME  "my_i2c_class"
#define BUFFER_SIZE 256
#define EEPROM_SIZE 32768

struct my_i2c_dev {
    struct i2c_client *client;
    struct cdev cdev;
    dev_t dev_num;
    struct mutex lock;
    u8 buf[BUFFER_SIZE];
};

extern struct class *my_i2c_class;
extern const struct file_operations my_i2c_fops;

/* Hardware Interface Prototypes */
ssize_t at24c256_hw_read(struct my_i2c_dev *dev, u8 *buf, size_t len, loff_t off);
ssize_t at24c256_hw_write(struct my_i2c_dev *dev, const u8 *buf, size_t len, loff_t off);

#endif /* AT24C256_H */