/* 
   PCI Driver 
   5-2-18
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/usb.h>
#include <linux/slab.h>
#include <linux/timer.h>

/* char driver */
#define DEVCNT 1
#define DEV_NAME  "ece_led"
#define DEV_CLASS "char_class"

/* pci driver */
#define PCI_DEVICE_82545EM_D 0x100F
#define PCI_DEVICE_82540EM_V 0x8086
#define LED_CNTRL_REG        0x00E00
#define DEV_CNTRL_REG        0x00000

/* char device class */
static struct class *char_class = NULL;

/* char device struct */
static struct mydev_dev {
    struct cdev cdev;
    dev_t mydev_node;
    umode_t mode;
} mydev;

/* pci driver name */
static char *driver_name = "my_pci_driver";

/* pci table struct */
static const struct pci_device_id my_pci_tbl[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_82545EM_D), 0, 0, 0},

	/* last entry */
	{0, }
};
MODULE_DEVICE_TABLE(pci, my_pci_tbl);
MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Ryan Bornhorst");

/* stores the contents of the led cntrl register */
uint32_t led_reg;
static int blink_rate = 2;
module_param(blink_rate, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);

/* pci struct */
struct mydev_s {
	struct pci_dev *pdev;
	void *hw_addr;
};

/* global pci struct variable */
struct mydev_s *devs;

/* timer struct */
static struct timer_list my_timer;

/******************************************************************************

FUNCTIONS

******************************************************************************/

/* timer callback */
void timer_cb(unsigned long data) {

	uint32_t config;
	config = readl(devs->hw_addr + LED_CNTRL_REG);
	if(config != 0x0000000E) {
		config = 0xE;
		writel(config, devs->hw_addr + LED_CNTRL_REG);
	}
	else {
		config = 0xF;
		writel(config, devs->hw_addr + LED_CNTRL_REG);
	}
	data = blink_rate*2;
	mod_timer(&my_timer, HZ/data + jiffies);

}

/* allows device to be opened using open sys call */
static int dev_open(struct inode *inode, struct file *file) {

    printk(KERN_INFO "opening char device..\n");

    led_reg = readl(devs->hw_addr + LED_CNTRL_REG);
    printk(KERN_INFO "led_reg = 0x%08x blink_rate = %d\n", led_reg, blink_rate);

    setup_timer(&my_timer, timer_cb, blink_rate);

    mod_timer(&my_timer, HZ/blink_rate + jiffies);

    return 0;
}

/* allows device to be read from using read sys call */
static ssize_t dev_read(struct file *file, char __user *buf, 
                        size_t len, loff_t *offset) {
    int ret;
 
    if(*offset >= sizeof(int))
        return 0;

    if(!buf) {
        ret = -EINVAL;
        goto out;
    }

    if(copy_to_user(buf, &blink_rate, sizeof(int))) {
        ret = -EFAULT;
        goto out;
    }
    ret = sizeof(int);
    *offset += len;

    printk(KERN_INFO "User read from us %d...\n", blink_rate);
 
out:
    return ret;
}

/* allows device to be written to using write sys call */
static ssize_t dev_write(struct file *file, const char __user *buf, 
             		 size_t len, loff_t *offset) {

    int ret;

    if(!buf) {
        ret = -EINVAL;
        goto out;
    } 
      
    if(copy_from_user(&blink_rate, buf, len)) {
        ret = -EFAULT;
	printk(KERN_ERR "bad copy from user...\n");
        goto out;
    }

    ret = len;
    
    printk(KERN_INFO "userspace wrote %d to us...\n", blink_rate);
    
    if(blink_rate < 0) {
	ret = -EINVAL;
	blink_rate = 2;
	printk(KERN_ERR "negative blink rate not possible.. blink_rate = 2\n");
    } else if(blink_rate == 0) {
	printk(KERN_INFO "blink rate must be greater than 0.. blink_rate = 2\n");
	blink_rate = 2;
    } else {
	printk(KERN_INFO "blink_rate is now %d\n", blink_rate);
    }

out:
    return ret;
}

/* releases the device with the close() sys call */
static int dev_release(struct inode *inode, struct file *file) {

    printk(KERN_INFO "device was released...\n");
    del_timer_sync(&my_timer);
    return 0;
}

/* struct for file ops */
static struct file_operations mydev_fops = {
    .owner   = THIS_MODULE,
    .open    = dev_open,
    .read    = dev_read,
    .write   = dev_write,
    .release = dev_release,
};

/* function that allows access to device permissions */
static char *my_devnode(struct device *dev, umode_t *mode) {

    if(!mode)
	return NULL;

    /* give r/w permission to users */
    if(dev -> devt == mydev.mydev_node)
        *mode = 0777;

    return NULL;
}	

/* pci probe function */
static int dev_probe(struct pci_dev *pdev, const struct pci_device_id *ent) {

	uint32_t ioremap_len;
	uint32_t config;
	int err;

	err = pci_enable_device_mem(pdev);
	if(err)
		return err;

	err = dma_set_mask(&pdev->dev, DMA_BIT_MASK(64));
	if(err) {
		dev_err(&pdev->dev, "DMA configuration failed...0x%x\n", err);
		goto err_dma;
	}

	err = pci_request_selected_regions(pdev, pci_select_bars(pdev, IORESOURCE_MEM),
					   driver_name);
	if(err) {
		dev_info(&pdev->dev, "pci_request_selected_regions failed...%d\n", err);
		goto err_pci_reg;
	}

	pci_set_master(pdev);

	devs = kzalloc(sizeof(*devs), GFP_KERNEL);
	if(!devs) {
		err = -ENOMEM;
		goto err_dev_alloc;
	}
	devs->pdev = pdev;
	pci_set_drvdata(pdev, devs);

	ioremap_len = min_t(int, pci_resource_len(pdev, 0), 1024);
	dev_info(&pdev->dev, "ioremap len 0x%08x\n", ioremap_len);
	devs->hw_addr = ioremap(pci_resource_start(pdev, 0), ioremap_len);
	if(!devs->hw_addr) {
		err = -EIO;
		dev_info(&pdev->dev, "ioremap(0x%04x, 0x%04x) failed: 0x%x\n", 
		         (unsigned int)pci_resource_start(pdev, 0),
			 (unsigned int)pci_resource_len(pdev, 0), err);
		goto err_ioremap;
	}

	config = readl(devs->hw_addr + DEV_CNTRL_REG);
	dev_info(&pdev->dev, "cntrl reg 0x%08x", config);
	config = readl(devs->hw_addr + LED_CNTRL_REG);
	dev_info(&pdev->dev, "led cntrl reg 0x%08x", config);
	config = readl(devs->hw_addr);
	dev_info(&pdev->dev, "mmio starts at 0x%08x", config);

	return 0;

err_ioremap:
	kfree(devs);
err_dev_alloc:
	pci_release_selected_regions(pdev, pci_select_bars(pdev, IORESOURCE_MEM));
err_pci_reg:
err_dma:
	pci_disable_device(pdev);
	return err;
}

/* removes pci device during unbind or rmmod */
static void dev_remove(struct pci_dev *pdev) {

	devs = pci_get_drvdata(pdev);

	dev_info(&pdev->dev, "removing pci device...\n");
    
	iounmap(devs->hw_addr);

	kfree(devs);

	pci_release_selected_regions(pdev, pci_select_bars(pdev, IORESOURCE_MEM));
	pci_disable_device(pdev);
}

/* struct for the pci device */
static struct pci_driver my_driver = {
	.name     = "e1000e",
	.id_table = my_pci_tbl,
	.probe    = dev_probe,
	.remove   = dev_remove,
};

/* function for insmod call */
static int __init hello_init(void) {

    int ret = 0;
    
    printk(KERN_INFO "pci module loading..\n");
 
    /* register the pci device */
    ret = pci_register_driver(&my_driver);
    
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
    char_class = class_create(THIS_MODULE, DEV_CLASS);
    /* change file permissions */
    char_class -> devnode = my_devnode;
    if(IS_ERR(char_class)) {
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
	class_unregister(char_class);
	class_destroy(char_class);
        printk(KERN_ERR "cdev add failed!\n");
        goto cdev_err;
    }

    /* create the device node */
    if(device_create(char_class, NULL, mydev.mydev_node, NULL, 
	             DEV_NAME) == NULL) {
	class_unregister(char_class);
	class_destroy(char_class);
	device_destroy(char_class, mydev.mydev_node);
	printk(KERN_ERR "failed to create device...\n");
	goto cdev_err;
    }

   
    return ret;

cdev_err:
    unregister_chrdev_region(mydev.mydev_node, DEVCNT);
    return ret;  

}

/* function for rmmod call */
static void __exit hello_exit(void) {

    /* unregister the pci device */
    pci_unregister_driver(&my_driver);
 
    device_destroy(char_class, mydev.mydev_node);
    class_unregister(char_class);
    class_destroy(char_class);
    unregister_chrdev_region(mydev.mydev_node, DEVCNT);
   
    printk(KERN_INFO "pci module unloaded..\n");
}

module_init(hello_init);
module_exit(hello_exit);
