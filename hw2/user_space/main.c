#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define CHAR_DEVICE "/dev/hw2_dev"
#define BUF_LEN     100

static char recv[BUF_LEN];
static char send[BUF_LEN];

int main()
{
int fd;
int ret = 0;
int val;

/* open device for reading */
fd = open(CHAR_DEVICE, O_RDWR);
if(fd < 0)
    printf("\nUnable to open device...\n");

/* read int value from char driver */
ret = read(fd, recv, sizeof(int));
if(ret < 0)
    printf("\nUnable to read device...\n");

memcpy(&val, recv, sizeof(int) * sizeof(char));

printf("\nRead from device: %d\n", val);

/* add 5 to the old value */
sprintf(send, "%d", val + 5);

printf("\nSending to device: %s\n", send);

close(fd);

/* open the device for writing */
fd = open(CHAR_DEVICE, O_RDWR);
if(fd < 0)
    printf("\nUnable to open device...\n");

/* write new value to device */
ret = write(fd, send, sizeof(send));
if(ret < 0)
    printf("\nUnable to write to device...\n");

/* read new value from device */
ret = read(fd, recv, sizeof(int));
if(ret < 0)
    printf("\nUnable to read device...\n");

memcpy(&val, recv, sizeof(int) * sizeof(char));

printf("\nRead %d back... after sending %s\n", val, send);

close(fd);

return 0;

}
