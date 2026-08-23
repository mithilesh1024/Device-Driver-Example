#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/cdev.h>

#define DEVICE_NAME "MY_GPIO"
#define MAX_GPIO 26

struct myDev {
  int direction;
  int value;
  int pin;
  struct device *dev;
};

static struct class *my_class;
static struct myDev *device[MAX_GPIO];
static DEFINE_MUTEX(dev_lock);

static ssize_t my_value_show(struct device *dev, 
                             struct device_attribute *attr, 
                             char *buff){
	pr_info("[%s] called %s\n",__func__,attr->attr.name);
	struct myDev *data = dev_get_drvdata(dev);
	if(strcmp(attr->attr.name, "direction") == 0){
	  return sprintf(buff, "%d", data->direction);
	}else if(strcmp(attr->attr.name, "value") == 0){
	  return sprintf(buff, "%d", data->value);
	}else if(strcmp(attr->attr.name, "pin") == 0){
	  return sprintf(buff, "%d", data->pin);
	}else {
	  pr_err("Unknown FILE");
	}
	return -ENOMEM;
}

static ssize_t my_value_store(struct device *dev, 
                             struct device_attribute *attr, 
                             const char *buff,
                             size_t count){
	pr_info("[%s] called %s\n",__func__,attr->attr.name);
	struct myDev *data = dev_get_drvdata(dev);
	if(strcmp(attr->attr.name, "direction") == 0){
	  sscanf(buff, "%d", &data->direction);
	}else if(strcmp(attr->attr.name, "value") == 0){
	  sscanf(buff, "%d", &data->value);
	}else if(strcmp(attr->attr.name, "pin") == 0){
	  sscanf(buff, "%d", &data->pin);
	}else {
	  pr_err("Unknown FILE");
	  return -ENOMEM;
	}
	return count;
}

static DEVICE_ATTR(direction, 0664, my_value_show, my_value_store);
static DEVICE_ATTR(value, 0664, my_value_show, my_value_store);
static DEVICE_ATTR(pin, 0444, my_value_show, NULL);

static ssize_t add_device_store(const struct class *class,
                                const struct class_attribute *attr,
                                const char *buff,
                                size_t count){
  mutex_lock(&dev_lock);
  int id;
  sscanf(buff, "%d\n", &id);
  
  if(id < MAX_GPIO && id > MAX_GPIO){
    pr_err("GPIO with %d not present",id);
    mutex_unlock(&dev_lock);
    return -ENOMEM;
  }
  
  struct myDev *newdev;
  newdev = kzalloc(sizeof(*newdev), GFP_KERNEL);
  if(!newdev){
          mutex_unlock(&dev_lock);
	  return -ENOMEM;
  }
  
  newdev->pin = id;
  newdev->value = 0;
  newdev->direction = 0;
  
  newdev->dev = device_create(my_class, NULL, 0, NULL, "%d", id);
  if(IS_ERR(newdev->dev)){
          pr_err("GPIO: Failed to create class\n");
          kfree(newdev);
          mutex_unlock(&dev_lock);
	  return -ENOMEM;
  }
  dev_set_drvdata(newdev->dev, newdev);
  
  int ret;
  ret = device_create_file(newdev->dev, &dev_attr_pin);
  ret |= device_create_file(newdev->dev, &dev_attr_value);
  ret |= device_create_file(newdev->dev, &dev_attr_direction);
  if(ret){
          pr_err("GPIO: Failed to create device file\n");
          kfree(newdev);
          mutex_unlock(&dev_lock);
	  return -ENOMEM;
  }
  
  device[id] = newdev;
  pr_info("GPIO: created %d\n",id);      
  mutex_unlock(&dev_lock);
	
  return count;
}

static ssize_t remove_device_store(const struct class *class, const struct class_attribute *attr, const char *buff, size_t count){
    mutex_lock(&dev_lock);
    int id;
    sscanf(buff, "%d\n", &id);
   
    if(id < MAX_GPIO && id > MAX_GPIO){
      pr_err("GPIO with %d not present",id);
      mutex_unlock(&dev_lock);
      return -ENOMEM;
    }
    
    device_remove_file(device[id]->dev, &dev_attr_pin);
    device_remove_file(device[id]->dev, &dev_attr_value);
    device_remove_file(device[id]->dev, &dev_attr_direction);
    device_destroy(my_class, 0);
    kfree(device[id]);
    device[id] = NULL;
    
    pr_info("GPIO %d removed successfully",id);
        
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
	
	int i;
   	for(i=0; i<MAX_GPIO;i++){
   	  device[i] = NULL;
   	}
	
	pr_info("GPIO: loaded\n");
    	return 0;
}

static void __exit cleanup(void){
   	printk(KERN_INFO "GPIO: EXITING KERNEL\n");
   	int i;
   	for(i=0; i<MAX_GPIO;i++){
   	  if(!device[i]) continue;
   	    device_remove_file(device[i]->dev, &dev_attr_pin);
            device_remove_file(device[i]->dev, &dev_attr_value);
            device_remove_file(device[i]->dev, &dev_attr_direction);
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
MODULE_DESCRIPTION("Sysfs GPIO Driver");

