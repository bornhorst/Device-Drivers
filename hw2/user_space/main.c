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
int fd[2];
int ret = 0;
int val;

/* open device for r/w */
fd[0] = open(CHAR_DEVICE, O_RDWR);
fd[1] = open(CHAR_DEVICE, O_RDWR);
if((fd[0] < 0) || (fd[1] < 0))
    printf("\nUnable to open device...\n");

/* read int value from char driver */
ret = read(fd[0], recv, sizeof(int));
if(ret < 0)
    printf("\nUnable to read device...\n");

memcpy(&val, recv, sizeof(int) * sizeof(char));

printf("\nRead from device: %d\n", val);

/* add 5 to the old value */
sprintf(send, "%d", val + 5);

printf("\nSending to device: %s\n", send);

/* write new value to device */
ret = write(fd[1], send, strlen(send));
if(ret < 0)
    printf("\nUnable to write to device...\n");

/* read new value from device */
ret = read(fd[1], recv, sizeof(int));
if(ret < 0)
    printf("\nUnable to read device...\n");

memcpy(&val, recv, sizeof(int) * sizeof(char));

printf("\nRead %d back... after sending %s\n", val, send);

close(fd[0]);
close(fd[1]);

return 0;

}
