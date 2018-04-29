#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define CHAR_DEVICE "/dev/char_dev"
#define BUF_LEN     100

static char recv[BUF_LEN];
static char send[BUF_LEN];

int main()
{
int fd;
int ret = 0;
unsigned int val;

/* open device for r/w */
fd = open(CHAR_DEVICE, O_RDWR);

if(fd < 0)
    printf("\nUnable to open device...\n");

/* read int value from char driver */
ret = read(fd, recv, sizeof(unsigned int));
if(ret < 0)
    printf("\nUnable to read device...\n");

memcpy(&val, recv, sizeof(unsigned int) * sizeof(char));

printf("\nRead from device: 0x%08x\n", val);

close(fd);

fd = open(CHAR_DEVICE, O_RDWR);

strcpy(send, "0xa09");

ret = write(fd, send, strlen(send) + 1);

close(fd);

return 0;

}
