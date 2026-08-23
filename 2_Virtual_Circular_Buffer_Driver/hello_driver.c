#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/cdev.h>

#define DEVICE_NAME "Mithilesh"
#define BUFFER_SIZE 10

static dev_t dev_num;
static struct cdev cdev;

static char buffer[BUFFER_SIZE];
static size_t size;
static int head;
static int tail; 

static int my_open(struct inode *inode, struct file *file) {

    // Allocate memory for this file instance
    memset(buffer, 0, BUFFER_SIZE);
    size = 0;

    // Store pointer in `file->private_data`
    printk(KERN_INFO "Device opened, buffer allocated for process: %d\n", current->pid);
    return 0;
}

static ssize_t my_read(struct file *file, char __user *buf, size_t len, loff_t *off) {
    printk(KERN_INFO "%s called buffer=%s\n",__func__,buffer);
    if((len == 0 ) || (head == 0)){
        printk(KERN_ALERT "Reading from empty\n");
        return -1;
    }
    if((tail + len) > head) {
        printk(KERN_ALERT "Reading ahead of head\n");
        return -1;
    }
    printk(KERN_INFO "len updated to %d\n",len);

    if(tail >=  BUFFER_SIZE){
        tail = 0;
    }

    if(tail + len > BUFFER_SIZE){
        int extra = abs(BUFFER_SIZE - tail - len);
        if(copy_from_user(buffer + tail, buf, len - extra)){
            return -EFAULT;
        }
        tail = 0;
        if(copy_from_user(buffer + tail, buf + (len - extra), extra)){
            return -EFAULT;
        }
        tail = extra;
    }else{
        if(copy_to_user(buf, buffer + tail, len)){
            return -EFAULT;
        }
        tail += len;
    }

    printk(KERN_INFO "tail updated to %d\n",tail);

    return len;
}

static ssize_t my_write(struct file *file, const char __user *buf, size_t len, loff_t *off) {
    printk(KERN_INFO "%s called\n",__func__);
    if(len == 0 ){
        return -1;
    }
    printk(KERN_INFO "len updated to %d\n",len);
    if(head >=  BUFFER_SIZE){
        head = 0;
    }

    if(head + len > BUFFER_SIZE){
        int extra = abs(BUFFER_SIZE - head - len);
        if(copy_from_user(buffer + head, buf, len - extra)){
            return -EFAULT;
        }
        head = 0;
        if(copy_from_user(buffer + head, (buf + (len - extra)), extra)){
            return -EFAULT;
        }
        head = extra;
    }else{
        if(copy_from_user(buffer + head, buf, len)){
            return -EFAULT;
        }
        head += len;
    }
    printk(KERN_INFO "buffer updated to %s\n",buffer);
    printk(KERN_INFO "head updated to %d\n",head);

    return len;

}

static int my_release(struct inode *inode, struct file *file) {
    printk(KERN_INFO "Device closed, buffer freed for process: %d\n", current->pid);
    return 0;
}

static struct file_operations fop = {
    .owner = THIS_MODULE,
    .open = my_open,
    .read = my_read,
    .write = my_write,
    .release = my_release,
};

static int __init my_driver_init(void){
    printk(KERN_INFO "%s starting",DEVICE_NAME);
    if(alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME) != 0){
        printk(KERN_ERR "Failed to allocate major and minor number");
        return -1;
    }
    
    cdev_init(&cdev, &fop);
    if(cdev_add(&cdev, dev_num, 1) < 0){
        unregister_chrdev_region(dev_num, 1);
        printk(KERN_ALERT "Failed to add cdev\n");
        return -1;
    }
    head = 0;
    tail = 0;
    printk(KERN_ALERT  "Major=%d Minor=%d\n",MAJOR(dev_num),MINOR(dev_num));
    return 0;
}

static void __exit cleanup(void){
    printk(KERN_INFO "EXITING KERNEL");
    unregister_chrdev_region(dev_num, 1);
}

module_init(my_driver_init);
module_exit(cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mithilesh");
MODULE_DESCRIPTION("Virtual Circular Buffer Driver");
