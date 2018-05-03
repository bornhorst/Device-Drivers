#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdint.h>

#define CHAR_DEVICE "/dev/char_dev"

int main()
{
int fd;
ssize_t readb;
ssize_t writeb;
uint32_t val;

/* open device for r/w */
fd = open(CHAR_DEVICE, O_RDWR);
if(fd < 0)
    printf("\nUnable to open device...\n");

/* read led_reg from char driver */
readb = read(fd, &val, sizeof(uint32_t));
if(readb < 0)
    printf("\nUnable to read device...%ld\n", readb);

printf("\nRead from device: 0x%08x %ld\n", val, readb);

/* turn on led0 */
val = 0xe;

writeb = write(fd, &val, sizeof(uint32_t));
if(writeb < 0)
    printf("\nUnable to write to device...\n");

printf("\nSent to device: 0x%08x %ld\n", val, writeb);

readb = read(fd, &val, sizeof(uint32_t));
if(readb < 0)
    printf("\nUnable to read from device...\n");

printf("\n2nd read from the device: 0x%08x %ld\n", val, readb);

val = 0xf;

sleep(2);

writeb = write(fd, &val, sizeof(uint32_t));
if(writeb < 0)
    printf("\nUnable to write to device...\n");

printf("\nSent to device: 0x%08x %ld\n", val, writeb);

close(fd);

return 0;

}
