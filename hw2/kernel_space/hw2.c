/* char device driver */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/usb.h>
#include <linux/slab.h>

#define DEVCNT 1
#define DEV_NAME  "hw2_dev"
#define DEV_CLASS "hw2class"

/* device class */
static struct class *hw2_class = NULL;

/* device struct */
static struct mydev_dev {
    struct cdev cdev;
    dev_t mydev_node;
    umode_t mode;
    int syscall_val;
} mydev;

/* sys/modules/hw2/parameters */
static int hw2_sys = 25;
module_param(hw2_sys, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(hw2_sys, "Device Integer");

/* allows device to be opened using open sys call */
static int hw2_open(struct inode *inode, struct file *file) {

    printk(KERN_INFO "opening hw2..\n");

    /* value that we are reading and writing to */
    mydev.syscall_val = hw2_sys;

    return 0;
}

/* allows device to be read from using read sys call */
static ssize_t hw2_read(struct file *file, char __user *buf, 
                        size_t len, loff_t *offset) {

    int ret;

    if(*offset >= sizeof(int))
        return 0;

    if(!buf){
        ret = -EINVAL;
        goto out;
    }

    if(copy_to_user(buf, &mydev.syscall_val, sizeof(int))) {
        ret = -EFAULT;
        goto out;
    }
    ret = sizeof(int);
    *offset += len;

    printk(KERN_INFO "User read from us %d...\n", mydev.syscall_val);
 
out:
    return ret;
}

/* allows device to be written to using write sys call */
static ssize_t hw2_write(struct file *file, const char __user *buf, 
             size_t len, loff_t *offset) {

    char *kern_buf;
    int ret;

    if(!buf) {
        ret = -EINVAL;
        goto out;
    } 

    kern_buf = kmalloc(len, GFP_KERNEL);

    if(!kern_buf) {
        ret = -ENOMEM;
	printk(KERN_ERR "bad malloc...\n");
        goto out;
    }

    if(copy_from_user(kern_buf, buf, len)) {
        ret = -EFAULT;
	printk(KERN_ERR "bad copy from user...\n");
        goto mem_out;
    }

    ret = len;
    
    kstrtoint(buf, 10, &mydev.syscall_val); 

    printk(KERN_INFO "Userspace wrote %d to us...\n", mydev.syscall_val);

mem_out:
    kfree(kern_buf);
out:
    return ret;
}

static int hw2_release(struct inode *inode, struct file *file) {

    printk(KERN_INFO "device was released...\n");
    return 0;
}

static struct file_operations mydev_fops = {
    .owner = THIS_MODULE,
    .open = hw2_open,
    .read = hw2_read,
    .write = hw2_write,
    .release = hw2_release,
};

/* function that allows access to device permissions */
static char *my_devnode(struct device *dev, umode_t *mode) {

    if(!mode)
	return NULL;

    /* give r/w permission to users */
    if(dev->devt == mydev.mydev_node)
        *mode = 0666;

    return NULL;
}

/* create and initialize device here */
static int __init char_device_init(void) {

    int ret = 0;
    
    printk(KERN_INFO "hw2 module loading..\n");
    
    /* dynamic device allocation */
    ret = alloc_chrdev_region(&mydev.mydev_node, 0, DEVCNT, 
			      DEV_NAME);
    if(ret) {
        printk(KERN_ERR "alloc_chrdev_region failed\n");
        return ret;
    }

    /* success at allocation */
    printk(KERN_INFO "allocated %d devices at major: %d minor: %d\n", 
           DEVCNT, MAJOR(mydev.mydev_node), MINOR(mydev.mydev_node));
    
    /* create the device class /sys/class */
    hw2_class = class_create(THIS_MODULE, DEV_CLASS);
    /* change file permissions */
    hw2_class -> devnode = my_devnode;
    if(IS_ERR(hw2_class)) {
	printk(KERN_ERR "problem creating device class...\n");
	goto cdev_err;
    }
    printk(KERN_INFO "device class registered successfully...\n");

    /* allow file operations on device */
    cdev_init(&mydev.cdev, &mydev_fops);
    mydev.cdev.owner = THIS_MODULE;

    /* add cdev to core with device number */
    mydev.mydev_node = MKDEV(MAJOR(mydev.mydev_node), 
		             MINOR(mydev.mydev_node));

    /* allow user to access device */
    ret = cdev_add(&mydev.cdev, mydev.mydev_node, DEVCNT);
    if(ret) {
	class_unregister(hw2_class);
	class_destroy(hw2_class);
        printk(KERN_ERR "cdev add failed!\n");
        goto cdev_err;
    }

    /* create the device node */
    if(device_create(hw2_class, NULL, mydev.mydev_node, NULL, 
	             DEV_NAME) == NULL) {
	class_unregister(hw2_class);
	class_destroy(hw2_class);
	device_destroy(hw2_class, mydev.mydev_node);
	printk(KERN_ERR "failed to create device...\n");
	goto cdev_err;
    }

    printk(KERN_INFO "module parameter [hw2_sys] is now %d...\n", hw2_sys);
    
    return ret;

cdev_err:
    unregister_chrdev_region(mydev.mydev_node, DEVCNT);
    return ret;    
}

/* clean up device here */
static void __exit char_device_exit(void) {

    device_destroy(hw2_class, mydev.mydev_node);
    class_unregister(hw2_class);
    class_destroy(hw2_class);
    unregister_chrdev_region(mydev.mydev_node, DEVCNT);

    printk(KERN_INFO "hw2 module unloaded..\n");

}

MODULE_AUTHOR("Ryan Bornhorst");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.2");

module_init(char_device_init);
module_exit(char_device_exit);
