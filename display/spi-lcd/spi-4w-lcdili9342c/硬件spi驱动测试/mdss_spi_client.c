/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spi/spi.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/qpnp/pin.h>
#include <linux/delay.h>

#define SPI_PANEL_COMMAND_LEN 1
static struct spi_device *mdss_spi_client;
static int dc_gpio;

int mdss_spi_tx_command(const void *buf)
{
	int rc = 0;
	struct spi_transfer t = {
		.tx_buf = buf,
		.len    = SPI_PANEL_COMMAND_LEN,
	};
	struct spi_message m;

	if (!mdss_spi_client) {
		pr_err("%s: spi client not available\n", __func__);
		return -EINVAL;
	}

	mdss_spi_client->bits_per_word = 8;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	/*pull down dc gpio indicate this is command*/
	gpio_set_value(dc_gpio, 0);
	rc = spi_sync(mdss_spi_client, &m);
	gpio_set_value(dc_gpio, 1);

	return rc;
}

int mdss_spi_tx_parameter(const void *buf, size_t len)
{
	int rc = 0;
	struct spi_transfer t = {
		.tx_buf = buf,
		.len    = len,
	};
	struct spi_message m;

	if (!mdss_spi_client) {
		pr_err("%s: spi client not available\n", __func__);
		return -EINVAL;
	}

	mdss_spi_client->bits_per_word = 8;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	rc = spi_sync(mdss_spi_client, &m);

	return rc;
}

int mdss_spi_tx_pixel(const void *buf, size_t len)
{
	int rc = 0;
	struct spi_transfer t = {
		.tx_buf = buf,
		.len    = len,
		};
	struct spi_message m;

	if (!mdss_spi_client) {
		pr_err("%s: spi client not available\n", __func__);
		return -EINVAL;
	}

	mdss_spi_client->bits_per_word = 16;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	rc = spi_sync(mdss_spi_client, &m);

	return rc;
}

void Address_set1(uint32_t x1,uint32_t y1,uint32_t x2,uint32_t y2)
{
	unsigned char cmd = 0 ;
	unsigned char para[20]={0};
	cmd = 0x2a ;mdss_spi_tx_command(&cmd);
	para[0] = x1>>8;para[1] = x1;para[2] = x2>>8;para[3] = x2;
	mdss_spi_tx_parameter(para,4);

   cmd = 0x2b ;mdss_spi_tx_command(&cmd);
   para[0] = y1>>8;para[1] = y1;para[2] = y2>>8;para[3] = y2;
   mdss_spi_tx_parameter(para,4);

   cmd = 0x2c ;mdss_spi_tx_command(&cmd);
}
#define LCD_W  320
#define LCD_H  240

void LCD_Clear1(uint16_t Color)
{

	/*
	unsigned char *buf;
	buf = kmalloc(320*240*2, GFP_KERNEL);
	Address_set1(0,0,LCD_W-1,LCD_H-1);
	
	memset(buf, 0x7D, sizeof(*buf));

	mdss_spi_tx_pixel(buf,sizeof(*buf));
	*/
	int  index;	
	unsigned char buff[2] = {0} ;
	buff[0] = Color>>8 ;
	buff[1] = Color ;
	Address_set1(0,0,LCD_W-1,LCD_H-1);
	for(index=0;index<LCD_W*LCD_H;index++)
	mdss_spi_tx_pixel(buff,2);
	
	
}

static int mdss_spi_client_probe(struct spi_device *spidev)
{
	int irq;
	int cs;
	int cpha, cpol, cs_high;
	u32 max_speed;
	struct device_node *np;
	unsigned char cmd = 0 ;
	unsigned char para[20]={0};

	irq = spidev->irq;
	cs = spidev->chip_select;
	cpha = (spidev->mode & SPI_CPHA) ? 1:0;
	cpol = (spidev->mode & SPI_CPOL) ? 1:0;
	cs_high = (spidev->mode & SPI_CS_HIGH) ? 1:0;
	max_speed = spidev->max_speed_hz;
	np = spidev->dev.of_node;
	pr_err("cs[%x] CPHA[%x] CPOL[%x] CS_HIGH[%x] Max_speed[%d]\n",
		cs, cpha, cpol, cs_high, max_speed);

	dc_gpio = of_get_named_gpio(np, "dc-gpio", 0);
	if (!gpio_is_valid(dc_gpio))
		pr_err("%s %d,spi panel dc gpio is not valid\n",
						__func__, __LINE__);


	if (gpio_request(dc_gpio, "dc-gpios"))
		pr_err("%s %d spi panel dc gpio_request failed\n",
						__func__, __LINE__);


	if (gpio_direction_output(dc_gpio, 1))
		pr_err("%s %d set spi panel dc gpio direction failed\n",
						__func__, __LINE__);

	mdss_spi_client = spidev;

	/*	SET_CS();
	SET_RST();
	mdelay(10);
	CLR_RST();
	mdelay(10);
	SET_RST();
	mdelay(120);
	CLR_CS();
	
	if (gpio_request(911+25, "rset-gpios"))
		pr_err("%s %d spi panel dc gpio_request failed\n",
						__func__, __LINE__);
	gpio_direction_output(911+25, 1);
	mdelay(10);
	gpio_direction_output(911+25, 0);
	mdelay(10);
	gpio_direction_output(911+25, 1);
	mdelay(120);
	*/
	
	cmd = 0x11 ;mdss_spi_tx_command(&cmd);
	mdelay(120); 
	
	cmd = 0xC8 ;mdss_spi_tx_command(&cmd);
	para[0] = 0xFF;para[1] = 0x93;para[2] = 0x42;
	mdss_spi_tx_parameter(para,3);
	
	cmd = 0xC0 ;mdss_spi_tx_command(&cmd);
	para[0] = 0x12;para[1] = 0x10;
	mdss_spi_tx_parameter(para,2);

	cmd = 0xC1 ;mdss_spi_tx_command(&cmd);
	para[0] = 0x01; 
	mdss_spi_tx_parameter(para,1);

	cmd = 0xC5 ;mdss_spi_tx_command(&cmd);
	para[0] = 0xe3; 
	mdss_spi_tx_parameter(para,1);

	cmd = 0xE0 ;mdss_spi_tx_command(&cmd);
	para[0] = 0x00;
	para[1] = 0x05;
	para[2] = 0x05;
	para[3] = 0x05;
	para[4] = 0x13;
	para[5] = 0x0a;
	para[6] = 0x30;
	para[7] = 0x9a;
	para[8] = 0x43;
	para[9] = 0x08;
	para[10] = 0x0f;
	para[11] = 0x0b;
	para[12] = 0x16;
	para[13] = 0x1a;
	para[14] = 0x0F;
	mdss_spi_tx_parameter(para,15);

	cmd = 0xE1 ;mdss_spi_tx_command(&cmd);
 	para[0]  = 0x00;
 	para[1]  = 0x25;
 	para[2]  = 0x29;
 	para[3]  = 0x04;
 	para[4]  = 0x11;
 	para[5]  = 0x07;
 	para[6]  = 0x3d;
 	para[7]  = 0x57;
	para[8]  = 0x4f;
	para[9]  = 0x05;
	para[10] = 0x0c;
	para[11] = 0x0a;
	para[12] = 0x3A;
	para[13] = 0x3A;
	para[14] = 0x0F;
	mdss_spi_tx_parameter(para,15);
	
	cmd = 0xB4 ;mdss_spi_tx_command(&cmd);
	para[0] = 0x02; 
	mdss_spi_tx_parameter(para,1);
	
	cmd = 0xB1 ;mdss_spi_tx_command(&cmd);
	para[0] = 0x00;para[1] = 0x18;
	mdss_spi_tx_parameter(para,2);

	cmd = 0x36 ;mdss_spi_tx_command(&cmd);
	para[0] = 0xC8; 
	mdss_spi_tx_parameter(para,1);

	cmd = 0x3A ;mdss_spi_tx_command(&cmd);
	para[0] = 0x55; 
	mdss_spi_tx_parameter(para,1);
	
	cmd = 0xF6 ;mdss_spi_tx_command(&cmd);
	para[0] = 0x01;para[1] = 0x00;para[2] = 0x00;
	mdss_spi_tx_parameter(para,3);

	cmd = 0x29 ;mdss_spi_tx_command(&cmd);
	mdelay(20); 
	pr_err("%s %d eliot mdss_spi_client_probe end\n",
						__func__, __LINE__);

	LCD_Clear1(0x07E0);
	/*
#define WHITE         	 0xFFFF
#define BLACK         	 0x0000	  
#define BLUE         	 0x001F  
#define BRED             0XF81F
#define GRED 			 0XFFE0
#define GBLUE			 0X07FF
#define RED           	 0xF800
#define MAGENTA       	 0xF81F
#define GREEN         	 0x07E0
#define CYAN          	 0x7FFF
#define YELLOW        	 0xFFE0
#define BROWN 			 0XBC40 //×ØÉ«
#define BRRED 			 0XFC07 //×ØºìÉ«
#define GRAY  			 0X8430 //»ÒÉ«
	*/
	return 0;
}


static const struct of_device_id mdss_spi_dt_match[] = {
	{ .compatible = "qcom,mdss-spi-client" },
	{},
};

static struct spi_driver mdss_spi_client_driver = {
	.probe = mdss_spi_client_probe,
	.driver = {
	.name = "mdss-spi-client",
	.owner  = THIS_MODULE,
	.of_match_table = mdss_spi_dt_match,
	},
};

static int __init mdss_spi_init(void)
{
	int ret;

	ret = spi_register_driver(&mdss_spi_client_driver);

	return 0;
}
module_init(mdss_spi_init);

static void __exit mdss_spi_exit(void)
{
	spi_register_driver(&mdss_spi_client_driver);
}
module_exit(mdss_spi_exit);

