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
int      fd;
ssize_t  readb;
ssize_t  writeb;
uint32_t head_tail;

/* open device for r/w */
fd = open(CHAR_DEVICE, O_RDWR);
if(fd < 0)
    printf("\nUnable to open device...\n");

readb = read(fd, &head_tail, sizeof(uint32_t));
if(readb < 0)
    printf("\nUnable to read device...%ld\n", readb);

printf("\nRead from device: 0x%08x\n", head_tail);

close(fd);

return 0;

}
