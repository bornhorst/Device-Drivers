/**********************************************************************
 *  ledmon.c
 *
 *  Following the LED register notes in the Intel datasheet for the
 *  82575eb network device, this code is meant to monitor and display
 *  the current status for the 3 LEDs on the back of the typical NIC.
 *
 **********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <pci/pci.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <linux/types.h>
#include <errno.h>

#define IFNAMSIZ 16

#define MEM_WINDOW_SZ  0x00010000

/* LED Control */
#define E1000_LEDCTL	0x00E00	/* LED Control - RW */

#define E1000_LEDCTL_LED0_SHIFT		0
#define E1000_LEDCTL_LED1_SHIFT		8
#define E1000_LEDCTL_LED2_SHIFT		16
#define E1000_LEDCTL_LED3_SHIFT		24

#define E1000_LEDCTL_MODE_MASK		0x0F
#define E1000_LEDCTL_IVRT		0x40
#define E1000_LEDCTL_BLINK		0x80

#define E1000_LEDCTL_MODE_LINK_10K	0x0
#define E1000_LEDCTL_MODE_LINK_100K	0x1
#define E1000_LEDCTL_MODE_LINK_UP	0x2
#define E1000_LEDCTL_MODE_ACTIVITY	0x3
#define E1000_LEDCTL_MODE_LINK_ACT	0x4
#define E1000_LEDCTL_MODE_LINK_10	0x5
#define E1000_LEDCTL_MODE_LINK_100	0x6
#define E1000_LEDCTL_MODE_LINK_1000	0x7
#define E1000_LEDCTL_MODE_PCIX		0x8
#define E1000_LEDCTL_MODE_FULL_DUP	0x9
#define E1000_LEDCTL_MODE_COLLISION	0xA
#define E1000_LEDCTL_MODE_BUS_SPEED	0xB
#define E1000_LEDCTL_MODE_BUS_SIZE	0xC
#define E1000_LEDCTL_MODE_PAUSED	0xD
#define E1000_LEDCTL_MODE_LED_ON	0xE
#define E1000_LEDCTL_MODE_LED_OFF	0xF

#define E1000_LEDCTL_INITIAL_VALUE  \
	E1000_LEDCTL_MODE_LINK_UP << E1000_LEDCTL_LED0_SHIFT   | \
	(E1000_LEDCTL_BLINK | E1000_LEDCTL_MODE_ACTIVITY)        \
		<< E1000_LEDCTL_LED1_SHIFT                     | \
	E1000_LEDCTL_MODE_LINK_100 << E1000_LEDCTL_LED2_SHIFT  | \
	E1000_LEDCTL_MODE_LINK_1000 << E1000_LEDCTL_LED3_SHIFT


volatile void *e1000e_mem;
char *portname;


/* map the network device into our memory space */
int open_dev(off_t base_addr, volatile void **mem)
{
	int fd;

	fd = open("/dev/mem", O_RDWR);
	if (fd < 0) {
		perror("open");
		return -1;
	}

	*mem = mmap(NULL, MEM_WINDOW_SZ, (PROT_READ|PROT_WRITE), MAP_SHARED, fd,
			base_addr);
	if (*mem == MAP_FAILED) {
		perror("mmap/readable - try rebooting with iomem=relaxed");
		close(fd);
		return -1;
	}

	return fd;
}

/* write to a device register */
void ew32(u32 reg, u32 value)
{
	u32 *p = (u32 *)(e1000e_mem + reg);
	*p = value;
}

/* read from a device register */
u32 er32(u32 reg)
{
	u32 *p = (u32 *)(e1000e_mem + reg);
	u32 v = *p;
	return v;
}

/* decode the led bits into on or off (true or false) */
bool get_led_state(u8 led_bits)
{
	bool led_state;
	FILE *input;
	char speed_buf[16];
	char filename[64];
	int speed = 0;
	u8 mode;

	/* If the 'speed' file exists, we have link */
	snprintf(filename, sizeof(filename), "/sys/class/net/%s/speed", portname);
	input = fopen(filename, "r");
	if (!input) {
		if (errno != ENOENT) {
			perror(filename);
			exit(1);
		}
	} else {
		fgets(speed_buf, sizeof(speed_buf), input);
		fclose(input);
		speed = atoi(speed_buf);
	}

	/* should the LED be on or off? */
	led_state = false;
	mode = led_bits & E1000_LEDCTL_MODE_MASK;
	switch (mode) {
	case E1000_LEDCTL_MODE_LINK_10K:
		led_state = (speed == 10 || speed == 1000);
		break;

	case E1000_LEDCTL_MODE_LINK_100K:
		led_state = (speed == 100 || speed == 1000);
		break;

	case E1000_LEDCTL_MODE_LINK_UP:
		led_state = (speed != 0);
		break;

	case E1000_LEDCTL_MODE_ACTIVITY:
		break;

	case E1000_LEDCTL_MODE_LINK_ACT:
		led_state = (speed != 0);
		break;

	case E1000_LEDCTL_MODE_LINK_10:
		led_state = (speed == 10);
		break;

	case E1000_LEDCTL_MODE_LINK_100:
		led_state = (speed == 100);
		break;

	case E1000_LEDCTL_MODE_LINK_1000:
		led_state = (speed == 1000);
		break;

	case E1000_LEDCTL_MODE_PCIX:
		break;

	case E1000_LEDCTL_MODE_FULL_DUP:
		break;

	case E1000_LEDCTL_MODE_COLLISION:
		break;

	case E1000_LEDCTL_MODE_BUS_SPEED:
		break;

	case E1000_LEDCTL_MODE_BUS_SIZE:
		break;

	case E1000_LEDCTL_MODE_PAUSED:
		break;

	case E1000_LEDCTL_MODE_LED_ON:
		led_state = true;
		break;

	case E1000_LEDCTL_MODE_LED_OFF:
		led_state = false;
		break;
	}

	if (led_bits & E1000_LEDCTL_IVRT)
		led_state = !led_state;

	if (led_bits & E1000_LEDCTL_BLINK)
		led_state = !led_state;

	return led_state;
}

/* print a simple ASCII representation of the 4 LEDs */
void print_leds()
{
	bool led0, led1, led2, led3;
	u8 led_bits;
	u32 ledctl;

	ledctl = er32(E1000_LEDCTL);

	led_bits = ledctl & 0xFF;
	led0 = get_led_state(led_bits);

	led_bits = (ledctl >> E1000_LEDCTL_LED1_SHIFT) & 0xFF;
	led1 = get_led_state(led_bits);

	led_bits = (ledctl >> E1000_LEDCTL_LED2_SHIFT) & 0xFF;
	led2 = get_led_state(led_bits);

	led_bits = (ledctl >> E1000_LEDCTL_LED3_SHIFT) & 0xFF;
	led3 = get_led_state(led_bits);

	printf("%s:   %c %c %c %c\r",
		portname,
		led0 ? '0' : '.',
		led1 ? '0' : '.',
		led2 ? '0' : '.',
		led3 ? '0' : '.');
	fflush(stdout);
}

int main(int argc, char **argv)
{
	int dev_mem_fd;
	char buf[128] = { 0 };
	char driver_name[24] = { 0 };
	char addr_str[24] = { 0 };
	bool readloop = false;
	off_t base_addr; 
	FILE *input;
	int ret = 0;
	int len;
	
	if (getuid() != 0) {
		printf("%s: must be run as root\n", argv[0]);
		exit(1);
	}

	if (argv[1] == NULL) {
		printf("Usage: %s <ethX> [-L]\n", argv[0]);
		exit(1);
	}
	portname = argv[1];
	if (strlen(portname) > IFNAMSIZ) {
		printf("%s: bad interface name\n", argv[0]);
		exit(1);
	}

	if (argc > 2 && strcmp(argv[2], "-L") == 0)
		readloop = true;

        /* find driver name to be sure we're using an e1000/e1000e device */
	snprintf(buf, sizeof(buf),
		 "basename `readlink /sys/class/net/%s/device/driver` 2>/dev/null",
		 portname);
	input = popen(buf, "r");
	if (!input) {
		perror("getting driver name");
		exit(1);
	}
	fgets(driver_name, sizeof(driver_name), input);
	fclose(input);

	len = strlen(driver_name);
	if (len <= 1) {
		fprintf(stderr, "%s driver not found\n", portname);
		exit(1);
	}
	if (strncmp(driver_name, "e1000", 5)) {
		fprintf(stderr, "%s wrong driver\n", driver_name);
		exit(1);
	}

        /* find the base memory address
	 * we just need the start address of resource[0], so only
	 * need to read the first line of the file.
	 */
        snprintf(buf, sizeof(buf),
                 "/sys/class/net/%s/device/resource", portname);
        input = fopen(buf, "r");
        if (!input) {
                perror("getting port mem info");
                exit(1);
        }
        fgets(addr_str, sizeof(addr_str), input);
        fclose(input);

        base_addr = strtol(addr_str, NULL, 0);
        if (len <= 1) {
                fprintf(stderr, "%s not found\n", portname);
                exit(1);
        }

	/* open and read memory */
	dev_mem_fd = open_dev(base_addr, &e1000e_mem);
	if (dev_mem_fd >= 0 && e1000e_mem) {
		/* printf("LEDCTL 0x%x\n", er32(E1000_LEDCTL)); */
		do {
			print_leds();
			usleep(250);
		} while (readloop);
		printf("\n");
	}

	close(dev_mem_fd);
	munmap((void *)e1000e_mem, MEM_WINDOW_SZ);

	return ret;
}

