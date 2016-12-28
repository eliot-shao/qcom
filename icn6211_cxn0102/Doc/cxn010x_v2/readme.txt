add /dev/input/uevent2 cxn010x
add /dev/cxn010x file node for i2c read write interface 

test progarm:
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#define DEVICE_NAME "/dev/cxn010x"
int main(int argc, char** argv)
{
    int fd = -1;
    int tt[3]={0};
    char xx[3]={0};
    fd = open(DEVICE_NAME, O_RDWR);
    if(fd == -1) {
        printf("Failed to open device %s\n", DEVICE_NAME);
        return -1;
    }
    read(fd,xx,3);
    printf("read value %d,%d,%d\n",xx[0],xx[1],xx[2]);
    
		tt[0] = 2 ;
		tt[1] = 1 ;
 
    write(fd, tt, 2);//shutdown input
    
    close(fd);
    return 0;
}