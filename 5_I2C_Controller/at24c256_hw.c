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
    size_t bytes_left = len;
    size_t written_total = 0;
    loff_t cur_off = off;
    int ret;

    pr_info("at24c256_hw_write: Entered with len=%zu, off=%lld\n", len, off);

    if (cur_off >= AT24C256_TOTAL_SIZE) {
        pr_warn("at24c256_hw_write: Offset %lld exceeds total size %d. Returning 0\n",
                cur_off, AT24C256_TOTAL_SIZE);
        return 0;
    }

    if (cur_off + len > AT24C256_TOTAL_SIZE) {
        bytes_left = AT24C256_TOTAL_SIZE - cur_off;
        pr_info("at24c256_hw_write: Trimmed bytes_left to %zu (boundary check)\n", bytes_left);
    }

    while (bytes_left > 0) {
        u16 page_offset = cur_off % AT24C256_PAGE_SIZE;
        size_t space_in_page = AT24C256_PAGE_SIZE - page_offset;
        size_t chunk_len = min(bytes_left, space_in_page);

        pr_info("at24c256_hw_write: Loop iter -> cur_off=0x%04llx, page_off=%u, chunk_len=%zu, bytes_left=%zu\n",
                cur_off, page_offset, chunk_len, bytes_left);

        /* Check for signal before doing I2C operations */
        if (signal_pending(current)) {
            pr_err("at24c256_hw_write: Signal pending in loop. Aborting...\n");
            ret = -ERESTARTSYS;
            goto out_unlock;
        }

        /* Payload layout: 2-byte EEPROM address + chunk payload */
        dev->write_buf[0] = (u8)(cur_off >> 8);   /* MSB Address */
        dev->write_buf[1] = (u8)(cur_off & 0xFF); /* LSB Address */

        pr_info("at24c256_hw_write: Copying chunk of %zu bytes into write_buf...\n", chunk_len);
        
        /* 1. Validate pointers are not NULL */
        if (!dev->write_buf || !buf) {
            pr_err("at24c256_hw_write: NULL pointer detected (write_buf=%p, buf=%p)\n", dev->write_buf, buf);
            ret = -EINVAL;
            goto out_unlock;
        }

        /* 2. Validate destination buffer capacity (prevent buffer overflow) */
        if ((chunk_len + 2) > BUFFER_SIZE) {
            pr_err("at24c256_hw_write: Target chunk %zu exceeds write_buf capacity %d\n", chunk_len + 2, BUFFER_SIZE);
            ret = -ENOBUFS;
            goto out_unlock;
        }

        /* 3. Safe to copy directly in kernel space */
        memcpy(&dev->write_buf[2], buf + written_total, chunk_len);

        pr_info("at24c256_hw_write: Sending i2c_master_send (%zu bytes total including address)\n", chunk_len + 2);
        ret = i2c_master_send(dev->client, dev->write_buf, chunk_len + 2);
        if (ret < 0) {
            dev_err(&dev->client->dev, "I2C page write failed at 0x%04llx: %d\n", cur_off, ret);
            goto out_unlock;
        }
        pr_info("at24c256_hw_write: i2c_master_send succeeded (sent %d bytes)\n", ret);

        pr_info("at24c256_hw_write: Waiting t_WR delay (%d ms)...\n", AT24C256_WRITE_TIME_MS);
        msleep(AT24C256_WRITE_TIME_MS);

        bytes_left -= chunk_len;
        written_total += chunk_len;
        cur_off += chunk_len;

        pr_info("at24c256_hw_write: Chunk done -> written_total=%zu, remaining=%zu\n", written_total, bytes_left);
    }

    off = cur_off;
    ret = written_total;

out_unlock:
    pr_info("at24c256_hw_write: Exiting loop. Unlocking mutex. written_total=%zu, ret=%d\n", written_total, ret);
    return (written_total > 0) ? written_total : ret;
}