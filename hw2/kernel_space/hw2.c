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
#define DEV_NAME  "hw2"
#define DEV_CLASS "hw2_class"

/* device class */
static struct class *hw2_class = NULL;

/* device struct */
static struct mydev_dev {
    struct cdev cdev;
    dev_t mydev_node;
    int syscall_val;
} mydev;

/* sys/modules/hw2/parameters */
static int hw2_sys = 25;
module_param(hw2_sys, int, S_IRUSR | S_IWUSR);

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
//    int temp_result;

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
/*
    ret = kstrtoint(buf, 10, &temp_result);
    if(ret != 0) {
	printk(KERN_ERR "bad int conversion..\n");
	return ret;
    }
*/
    printk(KERN_INFO "User got from us %d.\n", mydev.syscall_val);
 
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
        goto out;
    }

    if(copy_from_user(kern_buf, buf, len)) {
        ret = -EFAULT;
        goto mem_out;
    }

    ret = len;

    printk(KERN_INFO "Userspace wrote %s to us.\n", kern_buf);

mem_out:
    kfree(kern_buf);
out:
    return ret;
}

static struct file_operations mydev_fops = {
    .owner = THIS_MODULE,
    .read = hw2_read,
    .open = hw2_open,
    .write = hw2_write,
};

/* create and initialize device here */
static int __init char_device_init(void) {

    int ret = 0;
    dev_t curr_dev;
    
    printk(KERN_INFO "hw2 module loading..\n");

    ret = alloc_chrdev_region(&mydev.mydev_node, 0, DEVCNT, 
			      DEV_NAME);
    if(ret){
        printk(KERN_ERR "alloc_chrdev_region failed\n");
        return ret;
    }

    printk(KERN_INFO "allocated %d devices at major: %d minor: %d\n", 
           DEVCNT, MAJOR(mydev.mydev_node), MINOR(mydev.mydev_node));
    
    hw2_class = class_create(THIS_MODULE, DEV_CLASS);

    cdev_init(&mydev.cdev, &mydev_fops);
    mydev.cdev.owner = THIS_MODULE;

    curr_dev = MKDEV(MAJOR(mydev.mydev_node), 
		           MINOR(mydev.mydev_node));


    ret = cdev_add(&mydev.cdev, curr_dev, DEVCNT);
    if(ret) {
	class_destroy(hw2_class);
        printk(KERN_ERR "cdev add failed!\n");
        goto cdev_err;
    }

    if(device_create(hw2_class, NULL, curr_dev, NULL, 
		     DEV_NAME) == NULL) {
	class_destroy(hw2_class);
	device_destroy(hw2_class, mydev.mydev_node);
	printk(KERN_ERR "failed to create device...\n");
	goto cdev_err;
    }

    return ret;

cdev_err:
    unregister_chrdev_region(mydev.mydev_node, DEVCNT);
    return ret;    
}

/* clean up device here */
static void __exit char_device_exit(void) {

    cdev_del(&mydev.cdev);
    device_destroy(hw2_class, mydev.mydev_node);
    class_destroy(hw2_class);
    unregister_chrdev_region(mydev.mydev_node, DEVCNT);

    printk(KERN_INFO "hw2 module unloaded..\n");

}

MODULE_AUTHOR("Ryan Bornhorst");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.2");

module_init(char_device_init);
module_exit(char_device_exit);
