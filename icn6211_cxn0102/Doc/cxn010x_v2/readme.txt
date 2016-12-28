add /dev/input/uevent2 cxn010x
add /dev/cxn010x file node for i2c read write interface 

②创建/dev/cxn010x ,作为i2c读写设备的接口（专用于读写i2c0 cxn0102,最大可读写32个字节），我写了一个简单的测试代码见附件。

③创建/dev/input/event2 ，当cxn0102发出notify申请时候，通过uevent上报，通知应用程序读取cxn0102的数据。
  具体上报的事件，事件码，事件值如下：
        
④创建/sys/class/i2c-dev/i2c-0/device/0-0077/cxn_ctl，用于控制cxn0102是休眠，唤醒状态。操作方法：
     echo 0 >/sys/class/i2c-dev/i2c-0/device/0-0077/cxn_ctl
    echo 1 >/sys/class/i2c-dev/i2c-0/device/0-0077/cxn_ctl

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