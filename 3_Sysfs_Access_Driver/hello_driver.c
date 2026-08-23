#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/cdev.h>

#define DEVICE_NAME "Mithilesh"
#define MAX_DEVICE 4

struct myDev_data {
        int id;
	int value;
	int value2;
	struct device *dev;
};

static struct class *my_class;
static struct myDev_data *device[MAX_DEVICE];
static int device_count = 0;
static DEFINE_MUTEX(dev_lock);

static ssize_t my_value_show(struct device *dev, struct device_attribute *attr, char *buff){
	pr_info("[%s] called\n",__func__);
	struct myDev_data *data = dev_get_drvdata(dev);
	return sprintf(buff, "%d\n", data->value);
}

static ssize_t my_value_store(struct device *dev, struct device_attribute *attr, const char *buff, size_t count){
	pr_info("[%s] called\n",__func__);
	struct myDev_data *data = dev_get_drvdata(dev);
	sscanf(buff, "%d", &data->value);
	pr_info("mydrv: val set to %d for device %d\n",data->value, data->id);
	return count;
}

static ssize_t my_value_show2(struct device *dev, struct device_attribute *attr, char *buff){
	pr_info("[%s] called\n",__func__);
	struct myDev_data *data = dev_get_drvdata(dev);
	return sprintf(buff, "%d\n", data->value2);
}

static ssize_t my_value_store2(struct device *dev, struct device_attribute *attr, const char *buff, size_t count){
	pr_info("[%s] called\n",__func__);
	struct myDev_data *data = dev_get_drvdata(dev);
	sscanf(buff, "%d", &data->value2);
	pr_info("mydrv: val2 set to %d for device %d\n",data->value2, data->id);
	return count;
}

static DEVICE_ATTR(val, 0664, my_value_show, my_value_store);
static DEVICE_ATTR(val2, 0664, my_value_show2, my_value_store2);

static ssize_t add_device_store(const struct class *class,
                                const struct class_attribute *attr,
                                const char *buff,
                                size_t count){
	struct myDev_data *newdev;
	int id,ret;

	mutex_lock(&dev_lock);
	if(device_count >= MAX_DEVICE){
	  pr_err("mydev allocated max devices\n");
	  mutex_unlock(&dev_lock);
	  return -ENOMEM;
	}
	
	id = device_count;
	newdev = kzalloc(sizeof(*newdev), GFP_KERNEL);
        if(!newdev){
          mutex_unlock(&dev_lock);
	  return -ENOMEM;
        }
        
        newdev->id = id;
        newdev->value = 0;
        
        newdev->dev = device_create(my_class, NULL, 0, NULL, "mydevice%d", id);
        if(IS_ERR(newdev->dev)){
          pr_err("mydev: Failed to create class\n");
          kfree(newdev);
          mutex_unlock(&dev_lock);
	  return -ENOMEM;
        }
        
        dev_set_drvdata(newdev->dev, newdev);
        
        ret = device_create_file(newdev->dev, &dev_attr_val);
        ret |= device_create_file(newdev->dev, &dev_attr_val2);
        if(ret){
          pr_err("mydev: Failed to create device file\n");
          kfree(newdev);
          mutex_unlock(&dev_lock);
	  return -ENOMEM;
        }
        
        device[id] = newdev;
        device_count++;
        
        pr_info("mydev: created mydevice%d\n",id);
        
        mutex_unlock(&dev_lock);
	return count;
}

static ssize_t remove_device_store(const struct class *class, const struct class_attribute *attr, const char *buff, size_t count){
        int id;
        
        sscanf(buff, "%d", &id);
        mutex_lock(&dev_lock);
        
        if(id < 0 || id >= device_count || !device[id]){
            pr_err("mydev: invalid ID (%d)\n", id);
            mutex_unlock(&dev_lock);
	    return -ENOMEM;
        }
        
        device_remove_file(device[id]->dev, &dev_attr_val);
        device_remove_file(device[id]->dev, &dev_attr_val2);
        device_destroy(my_class, 0);
        kfree(device[id]);
        device[id] = NULL;
        device_count--;
        pr_info("Device removed successfully");
        
        mutex_unlock(&dev_lock);
        return count;
}

static CLASS_ATTR_WO(add_device);
static CLASS_ATTR_WO(remove_device);

static int __init my_driver_init(void){
    	printk(KERN_INFO "%s starting",DEVICE_NAME);

	my_class = class_create(DEVICE_NAME);
	
	int ret;
	ret = class_create_file(my_class, &class_attr_add_device);
	ret |= class_create_file(my_class, &class_attr_remove_device);
	
	if(ret){
	  pr_err("Failed to create class attributes");
	  class_destroy(my_class);
	  return ret;
	}
	
	pr_info("mydrv: loaded\n");
    	return 0;
}

static void __exit cleanup(void){
   	printk(KERN_INFO "EXITING KERNEL\n");
   	int i;
   	for(i=0; i<device_count;i++){
   	  if(!device[i]) continue;
   	  device_remove_file(device[i]->dev, &dev_attr_val);
   	  device_remove_file(device[i]->dev, &dev_attr_val2);
	  device_destroy(my_class, 0); 
	  kfree(device[i]);
   	}
	class_remove_file(my_class, &class_attr_add_device);
	class_remove_file(my_class, &class_attr_remove_device);
	class_destroy(my_class);
}

module_init(my_driver_init);
module_exit(cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mithilesh");
MODULE_DESCRIPTION("Sysfs Acess Driver");
