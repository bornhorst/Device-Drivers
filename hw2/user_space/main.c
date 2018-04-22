#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#define BUF_LEN 100

char recv[BUF_LEN];

int main()
{

int ret = 0;
int val;

int fd = open("/dev/hw2", O_RDONLY);
if(fd < 0)
    printf("\nUnable to open device...\n");

ret = read(fd, recv, sizeof(int));
if(ret < 0)
    printf("\nUnable to read device...\n");

memcpy(&val, recv, sizeof(int) * sizeof(char));

printf("\n\nRead from device: %d\n\n", val);

return 0;

}
