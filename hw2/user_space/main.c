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
int fd = 0;
int ret = 0;
int val;

//chmod(CHAR_DEVICE, S_IRUSR | S_IRGRP | S_IROTH);

fd = open(CHAR_DEVICE, O_RDWR);
if(fd < 0)
    printf("\nUnable to open device...\n");

ret = read(fd, recv, sizeof(int));
if(ret < 0)
    printf("\nUnable to read device...\n");

memcpy(&val, recv, sizeof(int) * sizeof(char));

printf("\nRead from device: %d\n", val);

sprintf(send, "%d", val + 5);

printf("\nSending to device: %s\n", send);

ret = write(fd, send, strlen(send));

ret = read(fd, recv, sizeof(int));

memcpy(&val, recv, sizeof(int) * sizeof(char));

printf("\nRead %d back... after sending %s\n", val, send);

return 0;

}
