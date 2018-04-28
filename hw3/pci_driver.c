/* pci driver 
   ryan bornhorst
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/pci.h>

#define PCI_DEVICE_82540EM_D 0x100e
#define PCI_DEVICE_82540EM_V 0x8086
#define LED_MASK
#define REG_CONFIG1 0x00000

static char *driver_name = "my_driver";

static const struct pci_device_id my_pci_tbl[] = {
	{ PCI_DEVICE(PCI_DEVICE_82540EM_V, PCI_DEVICE_82540EM_D), 0, 0, 0},

	/* last entry */
	{0, }
};
MODULE_DEVICE_TABLE(pci, my_pci_tbl);
MODULE_LICENSE("Dual BSD/GPL");

static int new_leds;
module_param(new_leds, int, 0);

struct mydev_s {
	struct pci_dev *pdev;
	void *hw_addr;
};

static int dev_probe(struct pci_dev *pdev, const struct pci_device_id *ent) {

	struct mydev_s *devs;
	u32 ioremap_len;
	u8 config1;
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
	devs->hw_addr = ioremap(pci_resource_start(pdev, 0), ioremap_len);
	if(!devs->hw_addr) {
		err = -EIO;
		dev_info(&pdev->dev, "ioremap(0x%04x, 0x%04x) failed: 0x%x\n", 
		         (unsigned int)pci_resource_start(pdev, 0),
			 (unsigned int)pci_resource_len(pdev, 0), err);
		goto err_ioremap;
	}

	config1 = readb(devs->hw_addr + REG_CONFIG1);
	dev_info(&pdev->dev, "config1 = 0x%02x\n", config1);

	/* add led stuff here */
	
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

static void dev_remove(struct pci_dev *pdev) {

	struct mydev_s *devs = pci_get_drvdata(pdev);

	iounmap(devs->hw_addr);

	kfree(devs);

	pci_release_selected_regions(pdev, pci_select_bars(pdev, IORESOURCE_MEM));
	pci_disable_device(pdev);
}

static struct pci_driver my_driver = {
	.name     = "my_pci_device",
	.id_table = my_pci_tbl,
	.probe    = dev_probe,
	.remove   = dev_remove,
};

static int __init hello_init(void) {
	int ret;
	printk(KERN_INFO "Hello, Kernel!\n");
	ret = pci_register_driver(&my_driver);
	return ret;
}

static void __exit hello_exit(void) {
	printk(KERN_INFO "Goodbye, Kernel!\n");
	pci_unregister_driver(&my_driver);
}

module_init(hello_init);
module_exit(hello_exit);
