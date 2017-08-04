/* Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/gpio.h>

#define RST  	(25+911)
#define CSX  	(10+911)
#define SDA  	(8+911)
#define SCL  	(11+911)
#define DCX  	(32+911)


#define LCD_W  320
#define LCD_H  240


#define CLR_A0()		gpio_direction_output(DCX, 0) 
#define SET_A0()		gpio_direction_output(DCX, 1)
#define CLR_RST()		gpio_direction_output(RST, 0) 					  
#define SET_RST()		gpio_direction_output(RST, 1) 
#define CLR_CS()        gpio_direction_output(CSX, 0) 
#define SET_CS()        gpio_direction_output(CSX, 1)
#define LCD_SCK_L()		gpio_direction_output(SCL, 0)
#define LCD_SCK_H()		gpio_direction_output(SCL, 1)
#define LCD_SDA_L()		gpio_direction_output(SDA, 0)
#define LCD_SDA_H()		gpio_direction_output(SDA, 1)



static void SPI_WRITE_TX( unsigned char input)
{
    unsigned char i;
    //CLR_CS();
	//ndelay(1);
    for(i=0; i<8; i++)			
    { 
        LCD_SCK_L();
		//ndelay(1);
        if(input & 0x80)
            LCD_SDA_H();
        else LCD_SDA_L();
		
        LCD_SCK_H();
		//ndelay(1);
        input <<= 1;			
    }
    //SET_CS();
	//ndelay(1);
}


void LCD_WR_DATA8(uint8_t da)
{
	SET_A0();//ndelay(1);
	SPI_WRITE_TX(da);
	ndelay(1);
}
void LCD_WR_DATA(int da)
{
    SET_A0();//ndelay(1);
	SPI_WRITE_TX((da>>8));
	ndelay(1);
	SPI_WRITE_TX(da);
	ndelay(1);

}	 
void LCD_WR_REG(uint8_t da)
{
	CLR_A0();//ndelay(1);
	SPI_WRITE_TX(da);
	ndelay(1);
}
 
void Address_set(uint32_t x1,uint32_t y1,uint32_t x2,uint32_t y2)
{
   LCD_WR_REG(0x2a);
   LCD_WR_DATA8(x1>>8);
   LCD_WR_DATA8(x1);
   LCD_WR_DATA8(x2>>8);
   LCD_WR_DATA8(x2);
  
   LCD_WR_REG(0x2b);
   LCD_WR_DATA8(y1>>8);
   LCD_WR_DATA8(y1); 
   LCD_WR_DATA8(y2>>8);
   LCD_WR_DATA8(y2);

   LCD_WR_REG(0x2C);
}

void LCD_Sque_Init(void)
{
   	//进行时序初始化
	SET_CS();
	SET_RST();
	mdelay(10);
	CLR_RST();
	mdelay(10);
	SET_RST();
	mdelay(120);
	CLR_CS();  //打开片选使能


LCD_WR_REG(0x11);
mdelay(120); 
LCD_WR_REG(0xC8); 
LCD_WR_DATA8(0xFF); 
LCD_WR_DATA8(0x93); 
LCD_WR_DATA8(0x42);

LCD_WR_REG(0xC0); 
LCD_WR_DATA8(0x12);
LCD_WR_DATA8(0x10);

LCD_WR_REG(0xC1);  //power control2 VGH/VGL
LCD_WR_DATA8(0x01);  //0x01 2015.01.16

LCD_WR_REG(0xC5);  //VCOM
LCD_WR_DATA8(0xe3);  //0xE3 2015.01.16

LCD_WR_REG(0xE0); 
LCD_WR_DATA8(0x00); 
LCD_WR_DATA8(0x05); 
LCD_WR_DATA8(0x05); 
LCD_WR_DATA8(0x05); 
LCD_WR_DATA8(0x13); 
LCD_WR_DATA8(0x0a); 
LCD_WR_DATA8(0x30); 
LCD_WR_DATA8(0x9a); 
LCD_WR_DATA8(0x43); 
LCD_WR_DATA8(0x08); 
LCD_WR_DATA8(0x0f); 
LCD_WR_DATA8(0x0b); 
LCD_WR_DATA8(0x16); 
LCD_WR_DATA8(0x1a); 
LCD_WR_DATA8(0x0F);
 
LCD_WR_REG(0xE1); 
LCD_WR_DATA8(0x00); 
LCD_WR_DATA8(0x25); 
LCD_WR_DATA8(0x29); 
LCD_WR_DATA8(0x04); 
LCD_WR_DATA8(0x11); 
LCD_WR_DATA8(0x07); 
LCD_WR_DATA8(0x3d); 
LCD_WR_DATA8(0x57); 
LCD_WR_DATA8(0x4f); 
LCD_WR_DATA8(0x05); 
LCD_WR_DATA8(0x0c); 
LCD_WR_DATA8(0x0a); 
LCD_WR_DATA8(0x3A); 
LCD_WR_DATA8(0x3A); 
LCD_WR_DATA8(0x0F);
LCD_WR_REG(0xB4);
LCD_WR_DATA8(0x02);

LCD_WR_REG(0xB1); //DIV, RTNA
LCD_WR_DATA8(0x00); 
LCD_WR_DATA8(0x18);

LCD_WR_REG(0x36); 
LCD_WR_DATA8(0xc8);

LCD_WR_REG(0x3A); 
LCD_WR_DATA8(0x55); 

LCD_WR_REG(0xf6); 
LCD_WR_DATA8(0x01);
LCD_WR_DATA8(0x00);
LCD_WR_DATA8(0x00); 

 
LCD_WR_REG(0x29); 
mdelay(20); 



}
//Color:要清屏的填充色

void LCD_Clear(uint16_t Color)
{
	int  index;	
	Address_set(0,0,LCD_W-1,LCD_H-1);
	for(index=0;index<76800;index++)
	{
		LCD_WR_DATA(Color);    
	}
}/*
//POINT_COLOR:此点的颜色
void LCD_DrawPoint(uint16_t x,uint16_t y)
{
	Address_set(x,y,x,y);//设置光标位置 
	LCD_WR_DATA(0x001F); 	    
} */

//在指定区域内填充指定颜色
//区域大小:
//  (xend-xsta)*(yend-ysta)
void LCD_Fill(uint16_t xsta,uint16_t ysta,uint16_t xend,uint16_t yend,uint16_t color)
{          
	uint16_t i,j; 
	Address_set(xsta,ysta,xend,yend);      //设置光标位置 
	for(i=ysta;i<=yend;i++)
	{													   	 	
		for(j=xsta;j<=xend;j++)LCD_WR_DATA(color);//设置光标位置 	    
	} 					  	    
} 

static DEFINE_MUTEX(register_mutex);

static int gp_ex_probe(struct platform_device *pdev)
{
	int ret = 0;
	printk(KERN_ERR "eliot :gp_ex_probe\n");
	ret = gpio_request(RST, "rst_n");
		if (ret) {
			pr_err("gpio request failed111\n");
			return ret;
		}
	ret = gpio_request(CSX, "csx_n");
		if (ret) {
			pr_err("gpio request failed111\n");
			return ret;
		}
	ret = gpio_request(SDA, "sda_n");
		if (ret) {
			pr_err("gpio request failed111\n");
			return ret;
		}
	ret = gpio_request(SCL, "scl_n");
		if (ret) {
			pr_err("gpio request failed111\n");
			return ret;
		}
	ret = gpio_request(DCX, "dcx_n");
		if (ret) {
			pr_err("gpio request failed111\n");
			return ret;
		}
	mutex_lock(&register_mutex);
	//SET_CS();
	//CLR_RST();
	//LCD_SCK_L();
	//LCD_SDA_L();
	LCD_Sque_Init();
	LCD_Clear(0xF800);
	//LCD_Fill(0,0,200,200,0x001F); 
	
	
	LCD_WR_REG(0x20);
	mdelay(120); 
	mutex_unlock(&register_mutex);
	 
	//mdelay(1000);
	printk(KERN_ERR "eliot :gp_ex_prob^end\n");
	
	return ret;

}

static int gp_ex_remove(struct platform_device *plat)
{

	return 0;
}

static struct of_device_id __attribute__ ((unused)) gp_ex_of_match[] = {
	{ .compatible = "spi-lcd", },
	{}
};

static struct platform_driver gp_ex_driver = {
	.probe = gp_ex_probe,
	.remove = gp_ex_remove,
	.driver = {
		.name = "spi-lcd",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(gp_ex_of_match),
	},
};

static int __init gp_ex_init(void)
{
	return platform_driver_register(&gp_ex_driver);
}

static void __exit gp_ex_exit(void)
{
	platform_driver_unregister(&gp_ex_driver);
}

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Eliot shao <http://blog.csdn.net/eliot_shao>");
MODULE_DESCRIPTION("Driver lcd spi");
MODULE_VERSION("1.0");

module_init(gp_ex_init);
module_exit(gp_ex_exit);
