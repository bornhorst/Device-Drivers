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
uint32_t head_tail;
uint16_t head, tail;

/* open device for r/w */
fd = open(CHAR_DEVICE, O_RDWR);
if(fd < 0)
    printf("\nUnable to open device...\n");

readb = read(fd, &head_tail, sizeof(uint32_t));
if(readb < 0)
    printf("\nUnable to read device...%ld\n", readb);

printf("\nRead from device: 0x%08x\n", head_tail);

head = (head_tail >> 16) & 0xFFFFFFFF;
tail = head_tail & 0xFFFF;

printf("head = %d  tail = %d\n", head, tail);

close(fd);

return 0;

}
