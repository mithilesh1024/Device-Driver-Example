#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/cdev.h>

#define DEVICE_NAME "mithilesh"
#define BUFFER_SIZE 256
static struct cdev cdev;
static dev_t dev_num;

static int major;

// **Per-instance data structure**
static char buffer[BUFFER_SIZE];
static size_t size;

// **open() function**
static int my_open(struct inode *inode, struct file *file) {

    // Allocate memory for this file instance
    memset(buffer, 0, BUFFER_SIZE);
    size = 0;

    // Store pointer in `file->private_data`
    printk(KERN_INFO "Device opened, buffer allocated for process: %d\n", current->pid);
    return 0;
}

static ssize_t my_read(struct file *file, char __user *buf, size_t len, loff_t *off) {
    printk(KERN_INFO "Reading data: %s", buffer);

    printk(KERN_INFO "Buffer size: %zu\n", size);

    if (size == 0) {
        return 0; // EOF
    }

    // Ensure not to exceed user buffer
    size_t to_copy = min(size - *off, len);
    printk(KERN_INFO "*off size: %zu\n", *off);

    printk(KERN_INFO "to_copy size: %zu\n", len);

    // if (to_copy == 0) {
    //     return 0; // Nothing to copy
    // }

    // Copy data from kernel buffer to user space
    if (copy_to_user(buf, buffer, size)) {
        return -EFAULT;
    }

    printk(KERN_INFO "Data copied to user space: %zu bytes\n", to_copy);

    // Update the offset after reading
    *off += to_copy;

    return size;
}

// **write() function**
static ssize_t my_write(struct file *file, const char __user *buf, size_t len, loff_t *off) {

    if (len > BUFFER_SIZE) 
        len = BUFFER_SIZE; // Limit write size

    // Handle the offset for writing data
    size_t to_write = min_t(size_t, len, BUFFER_SIZE - *off);  // Use min_t with explicit casting

    // Copy data from user-space to kernel buffer
    if (copy_from_user(buffer + *off, buf, to_write)) {
        return -EFAULT; // Error copying
    }
    printk(KERN_INFO "Writing data: %s", buffer);
    // Update the size and offset after writing
    *off += to_write;
    size = to_write;

    printk(KERN_INFO "Written %zu bytes to process buffer: %d\n", to_write, current->pid);

    return to_write;
}

// **release() function (cleanup per-instance data)**
static int my_release(struct inode *inode, struct file *file) {
    struct my_device_data *dev_data = file->private_data;
    
    if (dev_data) {
        kfree(dev_data);  // Free allocated memory
        file->private_data = NULL;
        printk(KERN_INFO "Device closed, buffer freed for process: %d\n", current->pid);
    }
    
    return 0;
}

// File operations structure
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = my_open,
    .write = my_write,
    .release = my_release,
    .read = my_read,
};

// **Module Initialization**
static int __init my_driver_init(void) {
    major = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (major < 0) {
        printk(KERN_ALERT "Failed to register character device\n");
        return major;
    }

    cdev_init(&cdev, &fops);
    if (cdev_add(&cdev, dev_num, 1) < 0) {
        unregister_chrdev_region(dev_num, 1);
        printk(KERN_ALERT "Failed to add cdev\n");
        return -1;
    }

    printk(KERN_INFO "Device registered with major number: %d minor:%d\n", MAJOR(dev_num), MINOR(dev_num));
    return 0;
}

// **Module Cleanup**
static void __exit my_driver_exit(void) {
    unregister_chrdev(major, DEVICE_NAME);
    printk(KERN_INFO "Device unregistered\n");
}

module_init(my_driver_init);
module_exit(my_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Character Device with Per-Instance Buffer Management");
