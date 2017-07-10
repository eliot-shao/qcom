#include <unistd.h>  
#include <stdio.h>  
#include <fcntl.h>  
#include <linux/fb.h>  
#include <sys/mman.h>  
#include <stdlib.h>
#include "yellow_face.zif"
int main()  
{  
    int fbfd = 0;  
    struct fb_var_screeninfo vinfo;  
    struct fb_fix_screeninfo finfo;  
    struct fb_cmap cmapinfo;  
    long int screensize = 0;  
    char *fbp = 0;  
    int x = 0, y = 0;  
    long int location = 0;  
    int b,g,r;  
    // Open the file for reading and writing  
    fbfd = open("/dev/graphics/fb0", O_RDWR,0);                    // 打开Frame Buffer设备  
    if (fbfd < 0) {  
                printf("Error: cannot open framebuffer device.%x\n",fbfd);  
                exit(1);  
    }  
    printf("The framebuffer device was opened successfully.\n");  
  
    // Get fixed screen information  
    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo)) {            // 获取设备固有信息  
                printf("Error reading fixed information.\n");  
                exit(2);  
    }  
    printf("\ntype:0x%x\n", finfo.type );                            // FrameBuffer 类型,如0为象素  
    printf("visual:%d\n", finfo.visual );                        // 视觉类型：如真彩2，伪彩3   
    printf("line_length:%d\n", finfo.line_length );        // 每行长度  
    printf("\nsmem_start:0x%lx,smem_len:%u\n", finfo.smem_start, finfo.smem_len ); // 映象RAM的参数  
    printf("mmio_start:0x%lx ,mmio_len:%u\n", finfo.mmio_start, finfo.mmio_len );  
      
    // Get variable screen information  
    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo)) {            // 获取设备可变信息  
                printf("Error reading variable information.\n");  
                exit(3);  
    }  
    printf("%dx%d, %dbpp,xres_virtual=%d,yres_virtual=%dvinfo.xoffset=%d,vinfo.yoffset=%d\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel,vinfo.xres_virtual,vinfo.yres_virtual,vinfo.xoffset,vinfo.yoffset);  

	screensize = finfo.line_length * vinfo.yres_virtual;
    // Map the device to memory 通过mmap系统调用将framebuffer内存映射到用户空间,并返回映射后的起始地址  
    fbp = (char *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED,fbfd, 0);  
    if ((int)fbp == -1) {  
                printf("Error: failed to map framebuffer device to memory.\n");  
                exit(4);  
    }  
    printf("The framebuffer device was mapped to memory successfully.\n");  
/***************exampel 1**********************/
	b = 10;
	g = 100;
	r = 100;
    for ( y = 0; y < 340; y++ )
        for ( x = 0; x < 420; x++ ) { 
      
         location = (x+100) * (vinfo.bits_per_pixel/8) + 
             (y+100) * finfo.line_length; 
      
         if ( vinfo.bits_per_pixel == 32 ) {        //          
                        *(fbp + location) = b; // Some blue  
                        *(fbp + location + 1) = g;             // A little green  
                        *(fbp + location + 2) = r;             // A lot of red  
                        *(fbp + location + 3) = 0;     // No transparency  
         }
      
        }
/*****************exampel 1********************/
/*****************exampel 2********************/		
 	unsigned char *pTemp = (unsigned char *)fbp;
	int i, j;
	//起始坐标(x,y),终点坐标(right,bottom)
	x = 400;
	y = 400;
	int right = 700;//vinfo.xres;
	int bottom = 1000;//vinfo.yres;
	
	for(i=y; i< bottom; i++)
	{
		for(j=x; j<right; j++)
		{
			unsigned short data = yellow_face_data[(((i-y)  % 128) * 128) + ((j-x) %128)];
			pTemp[i*finfo.line_length + (j*4) + 2] = (unsigned char)((data & 0xF800) >> 11 << 3);
			pTemp[i*finfo.line_length + (j*4) + 1] = (unsigned char)((data & 0x7E0) >> 5 << 2);
			pTemp[i*finfo.line_length + (j*4) + 0] = (unsigned char)((data & 0x1F) << 3);
		}
	} 
/*****************exampel 2********************/	
//note：vinfo.xoffset =0 vinfo.yoffset =0 否则FBIOPAN_DISPLAY不成功
	if (ioctl(fbfd, FBIOPAN_DISPLAY, &vinfo)) {    
                printf("Error FBIOPAN_DISPLAY information.\n");  
                exit(5);  
    }  
	sleep(10);
	munmap(fbp,finfo.smem_len);//finfo.smem_len == screensize == finfo.line_length * vinfo.yres_virtual 
    close(fbfd);  
    return 0;  
}  