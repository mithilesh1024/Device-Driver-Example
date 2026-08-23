// Pure I2C hardware operations (read/write/splits)

#include "at24c256.h"

ssize_t at24c256_hw_read(struct my_i2c_dev *dev, u8 *buf, size_t len, loff_t off)
{
    u8 addr_buf[2];
    u16 mem_addr = (u16)off;
    int ret;

    addr_buf[0] = (mem_addr >> 8) & 0xFF;
    addr_buf[1] = mem_addr & 0xFF;

    struct i2c_msg msgs[2] = {
        {
            .addr  = dev->client->addr,
            .flags = 0,
            .len   = 2,
            .buf   = addr_buf,
        },
        {
            .addr  = dev->client->addr,
            .flags = I2C_M_RD,
            .len   = len,
            .buf   = buf,
        }
    };

    ret = i2c_transfer(dev->client->adapter, msgs, 2);
    if (ret < 0) {
        dev_err(&dev->client->dev, "I2C read transfer failed: %d\n", ret);
        return ret;
    }

    return len;
}

ssize_t at24c256_hw_write(struct my_i2c_dev *dev, const u8 *buf, size_t len, loff_t off)
{
    u8 *write_buf;
    u16 mem_addr = (u16)off;
    int ret;

    write_buf = kmalloc(len + 2, GFP_KERNEL);
    if (!write_buf)
        return -ENOMEM;

    write_buf[0] = (mem_addr >> 8) & 0xFF;
    write_buf[1] = mem_addr & 0xFF;
    memcpy(&write_buf[2], buf, len);

    ret = i2c_master_send(dev->client, write_buf, len + 2);
    kfree(write_buf);

    if (ret < 0) {
        dev_err(&dev->client->dev, "I2C write failed: %d\n", ret);
        return ret;
    }

    /* EEPROM Internal Write Cycle Delay */
    msleep(5);

    return len;
}