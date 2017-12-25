#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#define DEVICE_NAME "/dev/vtp"
#define UPDATE_COORDINATE 	0x9981

struct coordinate {
	int x ;
	int y ;
	
};
struct coordinate test = {0,0};

int main(int argc, char** argv)
{
    int fd = -1;
	int i = 0 ;
    fd = open(DEVICE_NAME, O_RDWR);
    if(fd == -1) {
        printf("Failed to open device %s\n", DEVICE_NAME);
        return -1;
    }
	//上报坐标
	test.x = 510 ;
	test.y = 200;
    ioctl(fd,UPDATE_COORDINATE,&test);
 
    close(fd);
    return 0;
}