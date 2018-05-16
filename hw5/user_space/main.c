#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <getopt.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <pci/pci.h>
#include <linux/types.h>

#define DEV_MEM "/dev/mem"

#define LED_CTL 0x00E00

#define MEM_WINDOW_SZ 0x00010000

volatile void *e1000_mem;

void get_pci_info(off_t *base_addr) {
	
	struct pci_access *pacc;
	struct pci_dev *dev;
	//unsigned int c;
	char namebuf[1024], *name;

	pacc = pci_alloc();		/* Get the pci_access structure */
	/* Set all options you want -- here we stick with the defaults */
	pci_init(pacc);		/* Initialize the PCI library */
	pci_scan_bus(pacc);		/* We want to get the list of devices */

	for (dev=pacc->devices; dev; dev=dev->next)	/* Iterate over all devices */
	{
        	pci_fill_info(dev, PCI_FILL_IDENT | PCI_FILL_BASES | PCI_FILL_CLASS);
        	//c = pci_read_byte(dev, PCI_INTERRUPT_PIN);				

        	/* Look up full name of the device and get base address */
        	name = pci_lookup_name(pacc, namebuf, sizeof(namebuf), PCI_LOOKUP_DEVICE, 
				       dev->vendor_id, dev->device_id);

		/* get base address of Intel Ethernet Controller */
        	if((dev->vendor_id == 0x8086) && (dev->device_id == 0x100f)) {
      			printf(" (%s) (base0 = %lx)\n", name, dev->base_addr[0] - 0x4);
			*base_addr = dev->base_addr[0] - 0x4;
			printf("base_addr = %lx\n", *base_addr);
		}
	}

	pci_cleanup(pacc);		/* Close everything */
}

int open_dev(off_t base_addr, volatile void **mem) {
	
	int fd;

	fd = open(DEV_MEM, O_RDWR);
	if(fd < 0) {
		printf("unable to open /dev/mem\n");
		return -1;
	}

	*mem = mmap(NULL, MEM_WINDOW_SZ, (PROT_READ|PROT_WRITE),
		    MAP_SHARED, fd, base_addr);

	if(*mem == MAP_FAILED) {
		printf("mmap failed\n");
		close(fd);
		return -1;
	}
	
	return fd;
}

void wuint32(uint32_t reg, uint32_t value) {

	uint32_t *ptr = (uint32_t *)(e1000_mem + reg);
	*ptr = value;

}

uint32_t ruint32(uint32_t reg) {

	uint32_t *ptr = (uint32_t *)(e1000_mem + reg);
	uint32_t value = *ptr;
	return value;

}

int main() {

int dev_mem_fd;
off_t base_addr;
uint32_t led_ctl;
uint32_t led_on;
uint32_t led_off;

get_pci_info(&base_addr);

dev_mem_fd = open_dev(base_addr, &e1000_mem);
if(dev_mem_fd >= 0 && e1000_mem) {

	led_ctl = ruint32(LED_CTL);
	printf("led_ctl = 0x%08x\n", led_ctl);
	led_on  = 0xE;
	led_off = 0xF;

	for(int i = 0; i < 5; i++) { 
		wuint32(LED_CTL, led_on);
		sleep(1);
		wuint32(LED_CTL, led_off);
		sleep(1);
	}
}
close(dev_mem_fd);

munmap((void *)e1000_mem, MEM_WINDOW_SZ);

return 0;

}
