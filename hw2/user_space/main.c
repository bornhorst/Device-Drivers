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

/* open device for r/w */
fd = open(CHAR_DEVICE, O_RDWR);

if(fd < 0)
    printf("\nUnable to open device...\n");

/* read int value from char driver */
ret = read(fd, recv, sizeof(int));
if(ret < 0)
    printf("\nUnable to read device...\n");

memcpy(&val, recv, sizeof(int) * sizeof(char));

printf("\nRead from device: %d\n", val);

close(fd);

/* open device for r/w */
fd = open(CHAR_DEVICE, O_RDWR);

/* get new value from user */
printf("\nEnter an int value to send to the device:  ");
fgets(send, BUF_LEN, stdin);

printf("\n\nSending to device: %s\n", send);

/* write new value to device */
ret = write(fd, send, strlen(send));
if(ret < 0)
    printf("\nUnable to write to device...\n");

/* read new value from device */
ret = read(fd, recv, sizeof(int));
if(ret < 0)
    printf("\nUnable to read device...\n");

memcpy(&val, recv, sizeof(int) * sizeof(char));

printf("\nRead %d back... after sending %s\n\n", val, send);

close(fd);

return 0;

}
