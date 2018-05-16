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

#define LED_CTL        0x00E00
#define GOOD_PACK_RECV 0x04074

#define RESERVED       0x30303030

#define MEM_WINDOW_SZ  0x00010000

/* stores mmap for pci device */
volatile void *e1000_mem;

/* 
	scans all pci devices using pci library
	-use vendor and device id to find specific device

	I referenced an example of how to do this from the 
	pciutils library. 
	Found at github.com/gittup/pciutils/blob/gittup/example.c
*/
void get_pci_base_addr(off_t *base_addr) {
	
	struct pci_access *pacc;
	struct pci_dev *dev;
	char namebuf[1024], *name;
        
	/* allocate memory for pci_access struct */
	pacc = pci_alloc();		
	
	/* initialize pci lib utilities */
	pci_init(pacc);		
	
	/* scan pci bus for all devices it communicates with */
	pci_scan_bus(pacc);		

	/* run through devices until we find the one we are looking for */
	for (dev = pacc->devices; dev; dev = dev->next)
	{
        	pci_fill_info(dev, PCI_FILL_IDENT | PCI_FILL_BASES | PCI_FILL_CLASS);				

        	/* Look up full name of the device and get base address */
        	name = pci_lookup_name(pacc, namebuf, sizeof(namebuf), PCI_LOOKUP_DEVICE, 
				       dev->vendor_id, dev->device_id);

		/* get base address of Intel Ethernet Controller */
        	if((dev->vendor_id == 0x8086) && (dev->device_id == 0x100f)) {
      			printf("(%s) (base0 = 0x%lx)\n", name, dev->base_addr[0] - 0x4);
			*base_addr = dev->base_addr[0] - 0x4;
			printf("base_addr = 0x%lx\n", *base_addr);
		}
	}

	/* close pci access interface */
	pci_cleanup(pacc);		
}

/*
	open device using base address and map some memory to it

	I referenced this from ledmon.c
	-all credit goes to Shannon Nelson and Brett Creeley
*/
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
/*
	write to register at some offset
	
	I referenced this from ledmon.c
	-all credit goes to Shannon Nelson and Brett Creeley
*/
void wuint32(uint32_t reg, uint32_t value) {

	uint32_t *ptr = (uint32_t *)(e1000_mem + reg);
	*ptr = value;

}
/*
	read from a register at some offset

	I referenced this from ledmon.c
	-all credit goes to Shannon Nelson and Brett Creeley
*/
uint32_t ruint32(uint32_t reg) {

	uint32_t *ptr = (uint32_t *)(e1000_mem + reg);
	uint32_t value = *ptr;
	return value;

}

int main() {

int dev_mem_fd;
off_t base_addr;
uint32_t led_ctl;
uint32_t led_config;
uint32_t led0_on  = 0xE;
uint32_t led0_off = 0xF;
uint32_t led1_on  = 0xE00;
uint32_t led1_off = 0xF00;
uint32_t led2_on  = 0xE0000;
uint32_t led2_off = 0xF0000;
uint32_t led3_on  = 0xE000000;
uint32_t led3_off = 0xF000000;

/* grab that base address */
get_pci_base_addr(&base_addr);

/* open device and map memory for it */
dev_mem_fd = open_dev(base_addr, &e1000_mem);
if(dev_mem_fd >= 0 && e1000_mem) {

	/* read/modify/write led control register content */
	led_ctl = ruint32(LED_CTL);
	printf("led_ctl before blinking = 0x%08x\n", led_ctl);
	
	/* turn off leds that get set by ledmon, but don't write on reserved bits */
	led_config = led_ctl & RESERVED;
	wuint32(LED_CTL, led_config | (led0_off | led1_off | led2_off | led3_off));
	sleep(2);
	
	/* lets blinks some leds */
	printf("turning on led0 and led2\n");
	wuint32(LED_CTL, led_config | (led0_on | led2_on));
	sleep(2);
	printf("turning off all leds\n");
	wuint32(LED_CTL, led_config | (led0_off | led1_off | led2_off | led3_off));
	sleep(2);
	led_config = led_ctl & RESERVED;
	printf("blinking leds in descending order... 5 times\n");
	for(int i = 0; i < 5; i++) {
		wuint32(LED_CTL, led_config | led3_on);
		sleep(1);
		wuint32(LED_CTL, led_config | led3_off);
		wuint32(LED_CTL, led_config | led2_on);
		sleep(1);
		wuint32(LED_CTL, led_config | led2_off);
		wuint32(LED_CTL, led_config | led1_on);
		sleep(1);
		wuint32(LED_CTL, led_config | led1_off);
		wuint32(LED_CTL, led_config | led0_on);
		sleep(1);
		wuint32(LED_CTL, led_config | led0_off);
	}

	/* restore led_ctl register to initial value and read it */
	wuint32(LED_CTL, led_ctl);
	led_config = ruint32(LED_CTL);
	printf("led_ctl is restored to 0x%08x\n", led_config);

	/* try to read the good packet received register */
	led_config = ruint32(GOOD_PACK_RECV);
	printf("good packets received = 0x%08x\n", led_config);

}
close(dev_mem_fd);

munmap((void *)e1000_mem, MEM_WINDOW_SZ);

return 0;

}
