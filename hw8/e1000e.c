/* 
   Reading the buffers
   Homework #8 
   6-11-18
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
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>

/* char driver */
#define DEVCNT 1
#define DEV_NAME  "ece_led"
#define DEV_CLASS "char_class"

/* pci driver */
#define PCI_DEVICE_82545EM_D 0x100F
#define PCI_DEVICE_82540EM_V 0x8086
#define LED_CNTRL_REG        0x00E00
#define DEV_CNTRL_REG        0x00000
#define DEV_STATUS_REG       0x00008
#define CNTRL_LINK_UP	     0x1A41

/* receive packet registers */
#define RECV_CNTRL_REG       0x00100
#define RECV_RDBAL           0x02800
#define RECV_RDBAH	     0x02804
#define RECV_LEN             0x02808
#define RECV_HEAD            0x02810
#define RECV_TAIL	     0x02818
#define RECV_SETUP           0x821A

/* interrupts */
#define IMC		     0x000D8
#define IMS		     0x000D0
#define ICR		     0x000C0
#define IRQ_ENABLE           0x10

/* ring buffer */
#define RING_SIZE 16

/* descriptor stuff */
#define E1000_GET_DESC(R, i, type)  (&(((struct type *)((R).dma_mem))[i]))
#define E1000_RX_DESC(R, i)         E1000_GET_DESC(R, i, rx_desc)

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

/* pci struct */
struct mydev_s {
	struct pci_dev     *pdev;
	void               *hw_addr;
	struct work_struct service_task;
};

/* global pci struct variable */
struct mydev_s *devs;

/* receive descriptor */
struct rx_desc {
	__le64 buffer_addr;
	union {
		__le32 data;
		struct {
			__le16 length;
			__le16 css;
		} flags;
	} lower;
	union {
		__le32 data;
		struct {
			__u8 status;
			__u8 error;
			__le16 special;
		} field;
	} upper;
};

/* ring buffer info */
struct ring_buf {
	u8         *data;
	dma_addr_t dma_handle;
};

/* ring data struct */
struct rx_ring {
	void               *dma_mem;
	dma_addr_t         dma_handle;
	struct ring_buf    buffer[RING_SIZE];
	size_t             ring_size;
	uint16_t	   head;
	uint16_t	   tail;
} rx_ring;

/******************************************************************************

FUNCTIONS

******************************************************************************/

/* work thread */
static void service_task(struct work_struct *worker) {

	struct rx_ring *rx_rings = &rx_ring;
	struct rx_desc *rx_desc;
	int prev_head;

	rx_ring.head = readl(devs->hw_addr + RECV_HEAD);
	rx_ring.tail = readl(devs->hw_addr + RECV_TAIL);
		
	msleep(500);

	writel(0x0F0F0F0F, devs->hw_addr + LED_CNTRL_REG);

	do {	

		if(rx_ring.tail >= 15) 
			writel(0, devs->hw_addr + RECV_TAIL);
		else 
			writel((rx_ring.tail + 1), devs->hw_addr + RECV_TAIL);

		rx_ring.head = readl(devs->hw_addr + RECV_HEAD);
		rx_ring.tail = readl(devs->hw_addr + RECV_TAIL);

		/* find the ring position behind head */
		if(rx_ring.head == 0)
			prev_head = RING_SIZE - 1;
		else
			prev_head = rx_ring.head - 1;

		/* print out the dma address, dma descriptors, status, and length fields */
		pr_info("R[desc] [address 63:0]   [vl er S cks ln] [Status] [Length]\n");
	
		rx_desc = E1000_RX_DESC(*rx_rings, rx_ring.tail);
	
		pr_info("R[0x%02X] %016llX %08x%08x    %02x      %04x \n",
			rx_ring.tail, le64_to_cpu(rx_desc->buffer_addr),
			le32_to_cpu(rx_desc->upper.data),
			le32_to_cpu(rx_desc->lower.data),
			rx_desc->upper.field.status,
			rx_desc->lower.flags.length);
                
		/* clear the DD bits */
		if(rx_desc->upper.field.status == 0x03)
			rx_desc->upper.field.status = 0x00;

		/* print again just to show the DD bits were cleared out */
		pr_info("R[0x%02X] %016llX %08x%08x    %02x      %04x \n",
			rx_ring.tail, le64_to_cpu(rx_desc->buffer_addr),
			le32_to_cpu(rx_desc->upper.data),
			le32_to_cpu(rx_desc->lower.data),
			rx_desc->upper.field.status,
			rx_desc->lower.flags.length);

		if((rx_ring.tail % 2) == 0) 
			writel(0x0F0F0F0F, devs->hw_addr + LED_CNTRL_REG);
		else
			writel(0x0F0F0E0F, devs->hw_addr + LED_CNTRL_REG);
	
	/* continue until tail catches up to head */
	} while(rx_ring.tail != prev_head); 
}

/* interrupt handler */
static irqreturn_t irq_handler(int irq, void *data) {

	uint32_t interrupt;	

	/* led stuff */
	writel(0x0F0F0F0E, devs->hw_addr + LED_CNTRL_REG);

	/* init the work queue */
	schedule_work(&devs->service_task);

	/* read ICR reg to clear bit */
	interrupt = readl(devs->hw_addr + ICR);
		
	/* re-enable  IRQ */
	writel(IRQ_ENABLE, devs->hw_addr + IMS);

	return IRQ_HANDLED;
}

/* initialize the descriptor ring for dma */
static int ring_init(struct pci_dev *pdev) {

	int ret = 0;
	uint32_t config;
	int i;
	struct rx_ring *rxdr = &rx_ring;

	/* allocate memory for the ring struct */
	rxdr->ring_size = sizeof(struct rx_desc)*RING_SIZE;

	/* allocate contiguous memory for 16 descriptor rings */
	rxdr->dma_mem = dma_zalloc_coherent(&pdev->dev, rxdr->ring_size,
				 	    &rxdr->dma_handle, GFP_KERNEL);

	if(!rxdr->dma_mem) {
		ret = -1;
		goto err_nomem_dma_coherent;
	}

	/* set up high and low registers */
	config = (rxdr->dma_handle >> 32) & 0xFFFFFFFF;
	writel(config, devs->hw_addr + RECV_RDBAH);
	config = rxdr->dma_handle & 0xFFFFFFFF;
	writel(config, devs->hw_addr + RECV_RDBAL);

	/* set up head and tail */
	writel(16, devs->hw_addr + RECV_TAIL);
	writel(0, devs->hw_addr + RECV_HEAD);	

	/* set up receive length register */
	writel(rxdr->ring_size, devs->hw_addr + RECV_LEN);
	
	/* setup all the receive buffers: size = 2048 bytes */
	for(i = 0; i < RING_SIZE; i++) {
				
		struct rx_desc *rx_desc = E1000_RX_DESC(*rxdr, i);
		u8 *buf;
		
		/* allocate buffer for data */
		buf = kzalloc(2048, GFP_KERNEL);
		
		/* set up the data buffer */
		rxdr->buffer[i].data = buf;

		/* map the buffer address */
		rxdr->buffer[i].dma_handle = dma_map_single(&pdev->dev, buf, 
						            2048, DMA_FROM_DEVICE);

		if(dma_mapping_error(&pdev->dev, rxdr->buffer[i].dma_handle)) {
			ret = -1;
			goto err_nomem_dma_single;
		}

		/* store the buffer address */
		rx_desc->buffer_addr = cpu_to_le64(rxdr->buffer[i].dma_handle);
	}

	return ret;

err_nomem_dma_single:
	for(i = 0; i < RING_SIZE; i++) {
		dma_unmap_single(&pdev->dev, rxdr->buffer[i].dma_handle, 2048,
				 DMA_TO_DEVICE);
		kfree(rxdr->buffer[i].data);
	}

err_nomem_dma_coherent:
	dma_free_coherent(&pdev->dev, rxdr->ring_size, rxdr->dma_mem,
			  rxdr->dma_handle);
	return ret;
}

/* allows device to be opened using open sys call */
static int dev_open(struct inode *inode, struct file *file) {

    	printk(KERN_INFO "opening char device..\n");

       	return 0;
}

/* allows device to be read from using read sys call */
static ssize_t dev_read(struct file *file, char __user *buf, 
                        size_t len, loff_t *offset) {
	int ret;

	uint16_t head, tail;
	uint32_t config;

	head = readl(devs->hw_addr + RECV_HEAD);
	tail = readl(devs->hw_addr + RECV_TAIL);

	/* pack head and tail together */
	config =   head;
	config <<= 16;
	config |=  tail;

	printk(KERN_INFO "head/tail = 0x%08x\n", config);
 
    	if(*offset >= sizeof(uint32_t))
        	return 0;

    	if(!buf) {
        	ret = -EINVAL;
        	goto out;
    	}

    	if(copy_to_user(buf, &config, sizeof(uint32_t))) {
        	ret = -EFAULT;
        	goto out;
    	}
    	ret = sizeof(uint32_t);
    	*offset += len;

    	printk(KERN_INFO "User read from us 0x%08x...\n", config);
 
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
      
    	if(copy_from_user(&ret, buf, len)) {
        	ret = -EFAULT;
		printk(KERN_ERR "bad copy from user...\n");
        	goto out;
    	}

    	ret = len;
    
    	printk(KERN_INFO "userspace wrote 0x%08x to us...\n", ret); 
    
out:
    	return ret;
}

/* releases the device with the close() sys call */
static int dev_release(struct inode *inode, struct file *file) {

    	printk(KERN_INFO "device was released...\n");
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

	ioremap_len = pci_resource_len(pdev, 0);
	devs->hw_addr = ioremap(pci_resource_start(pdev, 0), ioremap_len);
	if(!devs->hw_addr) {
		err = -EIO;
		dev_info(&pdev->dev, "ioremap(0x%04x, 0x%04x) failed: 0x%x\n", 
		         (unsigned int)pci_resource_start(pdev, 0),
			 (unsigned int)pci_resource_len(pdev, 0), err);
		goto err_io_remap;
	}
	
	/* reset device */
	writel((1 << 26), devs->hw_addr + DEV_CNTRL_REG);
	udelay(5);

	/* force speed and duplex */
	writel(CNTRL_LINK_UP, devs->hw_addr + DEV_CNTRL_REG);

	/* set interrupts in IMS */
	writel(IRQ_ENABLE, devs->hw_addr + IMS);

	/* setup the receive ring */
	ring_init(pdev);

	/* setup receive cntrl reg */
	writel(RECV_SETUP, devs->hw_addr + RECV_CNTRL_REG);

	/* start work queue thread */
	INIT_WORK(&devs->service_task, service_task); 

	/* setup IRQ */
	err = request_irq(pdev->irq, irq_handler, 0, "e1000e_irq", devs);

	/* turn off all leds to start except 2 */
	writel(0x0F0E0F0F, devs->hw_addr + LED_CNTRL_REG);
	
	return 0;

err_dev_alloc:
	kfree(devs);
err_io_remap:
	pci_release_selected_regions(pdev, pci_select_bars(pdev, IORESOURCE_MEM));
err_pci_reg:
err_dma:
	pci_disable_device(pdev);
	return err;
}

/* removes pci device during unbind or rmmod */
static void dev_remove(struct pci_dev *pdev) {

	int i;
	struct rx_ring *rxdr = &rx_ring;

	cancel_work_sync(&devs->service_task);

	free_irq(pdev->irq, devs);

	if(rxdr->dma_mem && rxdr->buffer) {
	
		for(i = 0; i < RING_SIZE; i++) {
	
			if(rxdr->buffer[i].dma_handle) 
				dma_unmap_single(&pdev->dev,
					         rxdr->buffer[i].dma_handle,
						 2048, DMA_FROM_DEVICE);
	
			kfree(rxdr->buffer[i].data);
		}
	}

	if(rxdr->dma_mem) {

		dma_free_coherent(&pdev->dev, rxdr->ring_size, rxdr->dma_mem,
				  rxdr->dma_handle);
		rxdr->dma_mem = NULL;
	}

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
