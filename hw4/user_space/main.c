#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdint.h>

#define CHAR_DEVICE "/dev/ece_led"

int main()
{
int fd;
ssize_t readb;
ssize_t writeb;
int blink;

/* open device for r/w */
fd = open(CHAR_DEVICE, O_RDWR);
if(fd < 0)
    printf("\nUnable to open device...\n");

readb = read(fd, &blink, sizeof(int));
if(readb < 0)
    printf("\nUnable to read device...%ld\n", readb);

printf("\nRead from device: %d    %ld\n", blink, readb);

sleep(5);

printf("\nEnter a new blink rate: ");
scanf("%d", &blink);

writeb = write(fd, &blink, sizeof(int));
if(writeb < 0)
    printf("\nUnable to write to device...\n");

printf("\nSent to device: %d    %ld\n", blink, writeb);

sleep(5);

close(fd);

return 0;

}
