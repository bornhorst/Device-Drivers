/* char device driver */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/usb.h>
#include <linux/slab.h>

#define DEVCNT 5
#define DEV_NAME "hw2_dev"

/* device struct */
static struct mydev_dev{
    struct cdev cdev;
    dev_t mydev_node;
    int sys_int;
    int syscall_val;
} mydev;

/* sys/modules/hw2/parameters */
static int hw2_sys = 25;
static int hw2 = 15;
module_param(hw2, int, S_IRUSR | S_IWUSR);
module_param(hw2_sys, int, S_IRUSR | S_IWUSR);

static int hw2_open(struct inode *inode, struct file *file){

    printk(KERN_INFO "opening hw2..\n");

    mydev.sys_int = hw2;
    mydev.syscall_val = hw2_sys;

    return 0;
}

static ssize_t hw2_read(struct file *file, char __user *buf, 
            size_t len, loff_t *offset){

    int ret;

    if(*offset >= sizeof(int))
        return 0;

    if(!buf){
        ret = -EINVAL;
        goto out;
    }

    if(copy_to_user(buf, &mydev.syscall_val, sizeof(int))){
        ret = -EFAULT;
        printk(KERN_ERR "Bad copy_to_user...\n");
        goto out;
    }
    ret = sizeof(int);
    *offset += len;

    printk(KERN_INFO "User got from us %s.\n", buf);
 
out:
    return ret;
}

static ssize_t hw2_write(struct file *file, const char __user *buf, 
             size_t len, loff_t *offset){

    char *kern_buf;
    int ret;

    if(!buf){
        ret = -EINVAL;
        goto out;
    }

    kern_buf = kmalloc(len, GFP_KERNEL);

    if(!kern_buf){
        ret = -ENOMEM;
        goto out;
    }

    if(copy_from_user(kern_buf, buf, len)){
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

static int __init char_device_init(void){

    int ret = 0;
    
    printk(KERN_INFO "hw2 module loading..\n");

    ret = alloc_chrdev_region(&mydev.mydev_node, 0, DEVCNT, DEV_NAME);
    if(ret){
        printk(KERN_ERR "alloc_chrdev_region failed\n");
        return ret;
    }

    printk(KERN_INFO "allocated %d devices at major: %d minor: %d\n", 
           DEVCNT, MAJOR(mydev.mydev_node), MINOR(mydev.mydev_node));
    
    cdev_init(&mydev.cdev, &mydev_fops);
    mydev.cdev.owner = THIS_MODULE;

    ret = cdev_add(&mydev.cdev, mydev.mydev_node, DEVCNT);
    if(ret){
        printk(KERN_ERR "cdev add failed!\n");
        goto cdev_err;
    }

    return ret;

cdev_err:
    unregister_chrdev_region(mydev.mydev_node, DEVCNT);
    return ret;    
}

static void __exit char_device_exit(void){

    cdev_del(&mydev.cdev);

    unregister_chrdev_region(mydev.mydev_node, DEVCNT);

    printk(KERN_INFO "hw2 module unloaded..\n");

}

MODULE_AUTHOR("Ryan Bornhorst");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.2");

module_init(char_device_init);
module_exit(char_device_exit);
