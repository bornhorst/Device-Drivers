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
char *portname = "ens33";
char *pci_bus = "02:01";

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
char buf[128];
char pci_entry[128];
char addr_str[10];
off_t base_addr;
FILE *input;
int len;

uint32_t led_ctl;
uint32_t led_on;
uint32_t led_off;

snprintf(buf, sizeof(buf), "ip -br link show %s", portname);
input = popen(buf, "r");
if(!input) {
	printf("%s not correct\n", portname);
	exit(1);
}

fgets(buf, sizeof(pci_entry), input);
fclose(input);
if(strncmp(portname, buf, strlen(portname))) {
	printf("%s not found\n", portname);
	exit(1);
}

/* check for pci */
snprintf(buf, sizeof(buf), "lspci -s %s", pci_bus);
input = popen(buf, "r");
if(!input) {
	printf("%s not correct\n", pci_bus);
	exit(1);
}

fgets(pci_entry, sizeof(pci_entry), input);
fclose(input);
len = strlen(pci_entry);
if(len <= 1) {
	printf("%s not found\n", pci_bus);
	exit(1);
}

if(!strstr(pci_entry, "Ethernet controller") ||
   !strstr(pci_entry, "Intel")) {
	printf("%s wrong pci device\n", pci_entry);
	exit(1);
}

snprintf(buf, sizeof(buf),
	 "lspci -s %s -v | awk '/Memory at/ { print $3 }' | head -1", 
	 pci_bus);
input = popen(buf, "r");
if(!input) {
	printf("%s not correct device mem info\n", buf);
	exit(1);
}
fgets(addr_str, sizeof(addr_str), input);
fclose(input);

base_addr = strtol(addr_str, NULL, 16);
if(len <= 1) {
	printf("%s memory address invalid\n", addr_str);
	exit(1);
}

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
