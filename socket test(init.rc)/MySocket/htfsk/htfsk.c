#define SOCKET_NAME "htfsk"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <cutils/sockets.h>
#include <utils/Log.h>
#include <android/log.h>


int main(){
    char log[200]; 

    int connect_number = 6;
    int fdListen = -1, new_fd = -1;
    int ret;
    struct sockaddr_un peeraddr;
    socklen_t socklen = sizeof (peeraddr);
    int numbytes ;
    char buff[256];
    //这一步很关键，就是获取init.rc中配置的名为 "htfsk" 的socket
    fdListen = android_get_control_socket(SOCKET_NAME);
    if (fdListen < 0) {
	sprintf(log,"Failed to get socket '" SOCKET_NAME "' errno:%d", errno);
	__android_log_write(ANDROID_LOG_DEBUG,"FTM_JNI",log); 
	exit(-1);
    }
    //开始监听
    ret = listen(fdListen, connect_number);    
    
    sprintf(log,"Listen result %d",ret);
    __android_log_write(ANDROID_LOG_DEBUG,"FTM_JNI",log); 
    
    if (ret < 0) {
        perror("listen");
        exit(-1);
    }
    //等待Socket客户端发启连接请求
    new_fd = accept(fdListen, (struct sockaddr *) &peeraddr, &socklen);
    sprintf(log,"Accept_fd %d",new_fd);
    __android_log_write(ANDROID_LOG_DEBUG,"FTM_JNI",log); 
    if (new_fd < 0 ) {
        sprintf(log,"%d",errno);
        __android_log_write(ANDROID_LOG_DEBUG,"FTM_JNI",log); 
        perror("accept error");
        exit(-1);
    }
	
    while(1){
	//循环等待Socket客户端发来消息
	__android_log_write(ANDROID_LOG_DEBUG,"FTM_JNI","Waiting for receive");
	if((numbytes = recv(new_fd,buff,sizeof(buff),0))==-1)
	{
		sprintf(log,"%d",errno);
		__android_log_write(ANDROID_LOG_DEBUG,"FTM_JNI",log); 
		perror("recv");
		continue;
        }
	//发送消息回执给Socket客户端
	if(send(new_fd,buff,strlen(buff),0)==-1)
	{
		perror("send");
		close(new_fd);
		exit(0);
	}		
    }
    
    close(new_fd);
    close(fdListen);
    return 0;
}