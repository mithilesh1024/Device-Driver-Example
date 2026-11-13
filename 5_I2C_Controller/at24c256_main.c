// Module init/exit, i2c_driver probe/remove

#include "at24c256.h"

struct class *my_i2c_class;
// static struct i2c_client *at24c256_client;

static int my_i2c_probe(struct i2c_client *client)
{
    struct my_i2c_dev *dev;
    struct device *device;
    int ret;

    dev = devm_kzalloc(&client->dev, sizeof(*dev), GFP_KERNEL);
    if (!dev)
        return -ENOMEM;

    dev->client = client;
    mutex_init(&dev->lock);
    i2c_set_clientdata(client, dev);

    ret = alloc_chrdev_region(&dev->dev_num, 0, 1, DRIVER_NAME);
    if (ret < 0) {
        dev_err(&client->dev, "Failed to allocate chrdev region\n");
        return ret;
    }

    cdev_init(&dev->cdev, &my_i2c_fops);
    dev->cdev.owner = THIS_MODULE;
    ret = cdev_add(&dev->cdev, dev->dev_num, 1);
    if (ret < 0) {
        dev_err(&client->dev, "Failed to add cdev\n");
        goto err_unregister_chrdev;
    }

    /* Auto-creates /dev/my_i2c-80 */
    device = device_create(my_i2c_class, &client->dev, dev->dev_num, NULL, "my_i2c-%d", client->addr);
    if (IS_ERR(device)) {
        ret = PTR_ERR(device);
        dev_err(&client->dev, "Failed to create device file\n");
        goto err_cdev_del;
    }

    dev_info(&client->dev, "I2C driver probed successfully for addr 0x%02x\n", client->addr);
    return 0;

err_cdev_del:
    cdev_del(&dev->cdev);
err_unregister_chrdev:
    unregister_chrdev_region(dev->dev_num, 1);
    return ret;
}

static void my_i2c_remove(struct i2c_client *client)
{
    struct my_i2c_dev *dev = i2c_get_clientdata(client);

    device_destroy(my_i2c_class, dev->dev_num);
    cdev_del(&dev->cdev);
    unregister_chrdev_region(dev->dev_num, 1);

    dev_info(&client->dev, "I2C device removed\n");
}

static const struct of_device_id my_i2c_of_match[] = {
    {
        .compatible = "myproject,my-eeprom",
    },
    {}
};

MODULE_DEVICE_TABLE(of, my_i2c_of_match);

static const struct i2c_device_id my_i2c_id[] = {
    { "my_i2c_dev", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, my_i2c_id);

static struct i2c_driver my_i2c_driver = {
    .driver = {
        .name = DRIVER_NAME,
        .of_match_table = my_i2c_of_match,
    },
    .probe    = my_i2c_probe,
    .remove   = my_i2c_remove,
    .id_table = my_i2c_id,
};

static int __init my_driver_init(void)
{
    struct i2c_adapter *adapter;
    struct i2c_board_info board_info;
    int ret;

    my_i2c_class = class_create(CLASS_NAME);
    if (IS_ERR(my_i2c_class))
        return PTR_ERR(my_i2c_class);

    /* 1. Register the i2c_driver with kernel framework */
    ret = i2c_add_driver(&my_i2c_driver);
    if (ret < 0) {
        class_destroy(my_i2c_class);
        return ret;
    }

    // Below Commented part is not needed if using device tree
    // /* 2. Get handle for I2C Bus 1 (RPi default) */
    // adapter = i2c_get_adapter(1);
    // if (!adapter) {
    //     pr_err("Failed to get I2C adapter 1\n");
    //     i2c_del_driver(&my_i2c_driver);
    //     class_destroy(my_i2c_class);
    //     return -ENODEV;
    // }

    // /* 3. Define board info for 0x50 inside driver initialization */
    // memset(&board_info, 0, sizeof(struct i2c_board_info));
    // strscpy(board_info.type, "my_i2c_dev", I2C_NAME_SIZE);
    // board_info.addr = 0x50;

    // /* 4. Instantiate device on Bus 1 -> Triggers probe() automatically */
    // at24c256_client = i2c_new_client_device(adapter, &board_info);
    // i2c_put_adapter(adapter); /* Release adapter reference counter */

    // if (IS_ERR(at24c256_client)) {
    //     pr_err("Failed to instantiate I2C client at 0x50\n");
    //     i2c_del_driver(&my_i2c_driver);
    //     class_destroy(my_i2c_class);
    //     return PTR_ERR(at24c256_client);
    // }

    return 0;
}

static void __exit my_driver_exit(void)
{
    /* Unregister the client instance (triggers remove()) */
    // if (at24c256_client)
    //     i2c_unregister_device(at24c256_client);

    i2c_del_driver(&my_i2c_driver);
    class_destroy(my_i2c_class);
}

module_init(my_driver_init);
module_exit(my_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mithilesh");
MODULE_DESCRIPTION("Auto-instantiating AT24C256 Driver");