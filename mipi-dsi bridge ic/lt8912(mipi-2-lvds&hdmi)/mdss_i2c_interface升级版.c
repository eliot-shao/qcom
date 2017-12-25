/*
*author :shaomingliang (Eliot shao)
*version:1.0
*data:2016.9.9
*function description : an i2c interface for mipi bridge or others i2c slave
*/
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/workqueue.h>
#include <linux/errno.h>
#include <linux/pm.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>

#include "mdss_i2c_interface.h"
#include "mdss_dsi.h"

//#define TEST_PATTERN_MODE

#define _LVDS_Output_


enum
{
	H_act = 0,
	V_act,
	H_tol,
	V_tol,
	H_bp,
	H_sync,
	V_sync,
	V_bp
};


//------------------------------------------//

#ifdef _LVDS_Output_


// 设置LVDS屏的CLK：
	#define Panel_Pixel_CLK	8040	// 65MHz * 100

// 设置LVDS屏色深
	#define _8bit_ // 24 bit
//	#define _6bit_ // 18 bit

// 根据屏的规格书设置 LVDS 屏的Timing:
	static int LVDS_Panel_Timing[] = 
//	H_act	V_act	H_total	V_total	H_BP	H_sync	V_sync	V_BP
//	{1024,	600,	1344,	635,	140,	20, 	3,	20};// 1024x600 Timing
	{800,	1280,	1170,	1310,	220,	40, 	5,	20};// 1024x768 Timing

	union Temp
	{
	u8 Temp8[4];
	u16 Temp16[2];
	u32 Temp32;
	};

#endif

//------------------------------------------//

#define MIPI_Lane	4
//static int MIPI_Timing[]  = {1280,720,1650,750,220,40,5,20};// 1024x720 Timing
static int MIPI_Timing[]  = {800,1280,1170,1310,220,40,5,20};// 1024x720 Timing

struct mdss_i2c_interface {
	unsigned short flags;
	struct i2c_client *mdss_mipi_i2c_client;
	unsigned int slave_addr ;
	struct mutex lock;
	struct mutex i2c_lock;
	int gpio_rstn ;
	struct pinctrl		*pinctrl;
	struct pinctrl_state	*pin_default;
	struct pinctrl_state	*pin_sleep;
};

static struct mdss_i2c_interface *my_mipi_i2c ;


/*if I2C_TEST_OK == 1 stand for lt8912's i2c no ack , and the 
resource of my_mipi_i2c has been remove .must be stop here !
or will happen null pointer error !
*/

static int I2C_TEST_OK  = 0 ; //default
	
int HDMI_WriteI2C_Byte(int reg, int val)
{
	int rc = 0;
	rc = i2c_smbus_write_byte_data(my_mipi_i2c->mdss_mipi_i2c_client,reg,val);
	if (rc < 0) {
		printk("eliot :HDMI_WriteI2C_Byte fail \n");
        return rc;
		}
	return rc ;
}
int HDMI_ReadI2C_Byte(int reg)
{
	int val = 0;
	val = i2c_smbus_read_byte_data(my_mipi_i2c->mdss_mipi_i2c_client, reg);
    if (val < 0) {
        dev_err(&my_mipi_i2c->mdss_mipi_i2c_client->dev, "i2c read fail: can't read from %02x: %d\n", 0, val);
        return val;
    } 
	return val ;
}

int Reset_chip(void)
{
	// 拉低LT8912 的reset pin，delay 150 ms左右，再拉高
	gpio_direction_output(my_mipi_i2c->gpio_rstn, 1);
	mdelay(50);

	gpio_direction_output(my_mipi_i2c->gpio_rstn, 0);
	mdelay(150);

	gpio_direction_output(my_mipi_i2c->gpio_rstn, 1);

	return 0;
}
int test_read(void)
{
	int val1=0,val2=0,val3=0;
	printk("eliot :test_read start \n");
	mutex_lock(&my_mipi_i2c->i2c_lock);
	my_mipi_i2c->mdss_mipi_i2c_client->addr = 0x48;

	val1 = i2c_smbus_read_byte_data(my_mipi_i2c->mdss_mipi_i2c_client, 0x00);
	
	val2 = i2c_smbus_read_byte_data(my_mipi_i2c->mdss_mipi_i2c_client, 0x01);
	
	val3 = i2c_smbus_read_byte_data(my_mipi_i2c->mdss_mipi_i2c_client, 0x55);

	mutex_unlock(&my_mipi_i2c->i2c_lock);
	printk("eliot :test_read reg0x00:%d,reg0x01:%d,reg0x31:%d\n",val1,val2,val3);
	return 0 ;
	
}
#ifdef TEST_PATTERN_MODE

static void Test_pattern_1280x720(void)
{
	my_mipi_i2c->mdss_mipi_i2c_client->addr = 0x49;
	HDMI_WriteI2C_Byte(0x72,0x12);
	HDMI_WriteI2C_Byte(0x73,0x04);//RGD_PTN_DE_DLY[7:0]
	HDMI_WriteI2C_Byte(0x74,0x01);//RGD_PTN_DE_DLY[11:8]  192
	HDMI_WriteI2C_Byte(0x75,0x19);//RGD_PTN_DE_TOP[6:0]  41
	HDMI_WriteI2C_Byte(0x76,0x00);//RGD_PTN_DE_CNT[7:0]
	HDMI_WriteI2C_Byte(0x77,0xd0);//RGD_PTN_DE_LIN[7:0]
	HDMI_WriteI2C_Byte(0x78,0x25);//RGD_PTN_DE_LIN[10:8],RGD_PTN_DE_CNT[11:8]
	HDMI_WriteI2C_Byte(0x79,0x72);//RGD_PTN_H_TOTAL[7:0]
	HDMI_WriteI2C_Byte(0x7a,0xee);//RGD_PTN_V_TOTAL[7:0]
	HDMI_WriteI2C_Byte(0x7b,0x26);//RGD_PTN_V_TOTAL[10:8],RGD_PTN_H_TOTAL[11:8]
	HDMI_WriteI2C_Byte(0x7c,0x28);//RGD_PTN_HWIDTH[7:0]
	HDMI_WriteI2C_Byte(0x7d,0x05);//RGD_PTN_HWIDTH[9:8],RGD_PTN_VWIDTH[5:0]

	
	HDMI_WriteI2C_Byte(0x70,0x80);
	HDMI_WriteI2C_Byte(0x71,0x76);

	// 74.25M CLK
	HDMI_WriteI2C_Byte(0x4e,0x99);
	HDMI_WriteI2C_Byte(0x4f,0x99);
	HDMI_WriteI2C_Byte(0x50,0x69);
	HDMI_WriteI2C_Byte(0x51,0x80);

//-------------------------------------------------------//
}

#endif

/*This function para Get from FAE*/
int mdss_mipi_i2c_init(struct mdss_dsi_ctrl_pdata *ctrl, int from_mdp)
{
	//printk("eliot :mdss_mipi_i2c_init start \n");
	//Reset_chip();
	/*if I2C_TEST_OK == 1 stand for lt8912's i2c no ack , and the 
	resource of my_mipi_i2c has been remove .must be stop here !
	*/


	#ifdef _LVDS_Output_

	union Temp	Core_PLL_Ratio ;
	
	float f_DIV;

	#endif
	if(I2C_TEST_OK  == 1)
		return 0;
	// 往LT8912寄存器写值：
	//******************************************//
		mutex_lock(&my_mipi_i2c->i2c_lock);
	//	DigitalClockEn();
		my_mipi_i2c->mdss_mipi_i2c_client->addr = 0x48; // IIC address

#ifdef _LVDS_Output_
	HDMI_WriteI2C_Byte(0x08,0xff);// Register address : 0x08; 	Value : 0xff
	HDMI_WriteI2C_Byte(0x09,0xff);
	HDMI_WriteI2C_Byte(0x0a,0xff);
	HDMI_WriteI2C_Byte(0x0b,0x7c);// 
	HDMI_WriteI2C_Byte(0x0c,0xff);
#else
	HDMI_WriteI2C_Byte(0x08,0xff);// Register address : 0x08; 	Value : 0xff
	HDMI_WriteI2C_Byte(0x09,0x81);
	HDMI_WriteI2C_Byte(0x0a,0xff);
	HDMI_WriteI2C_Byte(0x0b,0x64);//
	HDMI_WriteI2C_Byte(0x0c,0xff);

	HDMI_WriteI2C_Byte(0x44,0x31);// Close LVDS ouput
	HDMI_WriteI2C_Byte(0x51,0x1f);
#endif
	//******************************************//
	
	//	TxAnalog();
		my_mipi_i2c->mdss_mipi_i2c_client->addr = 0x48;
	HDMI_WriteI2C_Byte(0x31,0xa1);
	HDMI_WriteI2C_Byte(0x32,0xa1);
	HDMI_WriteI2C_Byte(0x33,0x03);// 0x03 Open HDMI Tx； 0x00 Close HDMI Tx
	HDMI_WriteI2C_Byte(0x37,0x00);
	HDMI_WriteI2C_Byte(0x38,0x22);
	HDMI_WriteI2C_Byte(0x60,0x82);
	
	//******************************************//
	
	//	CbusAnalog();
		my_mipi_i2c->mdss_mipi_i2c_client->addr = 0x48;
	HDMI_WriteI2C_Byte(0x39,0x45);
	HDMI_WriteI2C_Byte(0x3b,0x00);
	//******************************************//
	
	//	HDMIPllAnalog();
	my_mipi_i2c->mdss_mipi_i2c_client->addr = 0x48;
	HDMI_WriteI2C_Byte(0x44,0x31);
	HDMI_WriteI2C_Byte(0x55,0x44);
	HDMI_WriteI2C_Byte(0x57,0x01);
	HDMI_WriteI2C_Byte(0x5a,0x02);
	
	//******************************************//
	
	//	MipiBasicSet();
		my_mipi_i2c->mdss_mipi_i2c_client->addr = 0x49;
	HDMI_WriteI2C_Byte(0x10,0x01); // 0x05 
	HDMI_WriteI2C_Byte(0x11,0x08); // 0x12 
	HDMI_WriteI2C_Byte(0x12,0x04);  
	HDMI_WriteI2C_Byte(0x13,MIPI_Lane%0x04);  // 00 4 lane  // 01 lane // 02 2 lane //03 3 lane
	HDMI_WriteI2C_Byte(0x14,0x00);  
	HDMI_WriteI2C_Byte(0x15,0x00);
	HDMI_WriteI2C_Byte(0x1a,0x03);  
	HDMI_WriteI2C_Byte(0x1b,0x03);  
	
#ifdef _LVDS_Output_
	my_mipi_i2c->mdss_mipi_i2c_client->addr = 0x48;
	HDMI_WriteI2C_Byte(0x44,0x31);
#endif
	
	//	MIPIDig1280x720();
		my_mipi_i2c->mdss_mipi_i2c_client->addr = 0x49;

	HDMI_WriteI2C_Byte(0x18,(u8)(MIPI_Timing[H_sync]%256)); // hwidth
	HDMI_WriteI2C_Byte(0x19,(u8)(MIPI_Timing[V_sync]%256)); // vwidth
	HDMI_WriteI2C_Byte(0x1c,(u8)(MIPI_Timing[H_act]%256)); // H_active[7:0]
	HDMI_WriteI2C_Byte(0x1d,(u8)(MIPI_Timing[H_act]/256)); // H_active[15:8]

	HDMI_WriteI2C_Byte(0x1e,0x67); // hs/vs/de pol hdmi sel pll sel
	HDMI_WriteI2C_Byte(0x2f,0x0c); // fifo_buff_length 12

	HDMI_WriteI2C_Byte(0x34,(u8)(MIPI_Timing[H_tol]%256)); // H_total[7:0]
	HDMI_WriteI2C_Byte(0x35,(u8)(MIPI_Timing[H_tol]/256)); // H_total[15:8]
	HDMI_WriteI2C_Byte(0x36,(u8)(MIPI_Timing[V_tol]%256)); // V_total[7:0]
	HDMI_WriteI2C_Byte(0x37,(u8)(MIPI_Timing[V_tol]/256)); // V_total[15:8]
	HDMI_WriteI2C_Byte(0x38,(u8)(MIPI_Timing[V_bp]%256)); // VBP[7:0]
	HDMI_WriteI2C_Byte(0x39,(u8)(MIPI_Timing[V_bp]/256)); // VBP[15:8]
	HDMI_WriteI2C_Byte(0x3a,(u8)((MIPI_Timing[V_tol]-MIPI_Timing[V_act]-MIPI_Timing[V_bp]-MIPI_Timing[V_sync])%256)); // VFP[7:0]
	HDMI_WriteI2C_Byte(0x3b,(u8)((MIPI_Timing[V_tol]-MIPI_Timing[V_act]-MIPI_Timing[V_bp]-MIPI_Timing[V_sync])/256)); // VFP[15:8]
	HDMI_WriteI2C_Byte(0x3c,(u8)(MIPI_Timing[H_bp]%256)); // HBP[7:0]
	HDMI_WriteI2C_Byte(0x3d,(u8)(MIPI_Timing[H_bp]/256)); // HBP[15:8]
	HDMI_WriteI2C_Byte(0x3e,(u8)((MIPI_Timing[H_tol]-MIPI_Timing[H_act]-MIPI_Timing[H_bp]-MIPI_Timing[H_sync])%256)); // HFP[7:0]
	HDMI_WriteI2C_Byte(0x3f,(u8)((MIPI_Timing[H_tol]-MIPI_Timing[H_act]-MIPI_Timing[H_bp]-MIPI_Timing[H_sync])/256)); // HFP[15:8]
	//	DDSConfig();
	my_mipi_i2c->mdss_mipi_i2c_client->addr = 0x49;
	HDMI_WriteI2C_Byte(0x4e,0x6A);
	HDMI_WriteI2C_Byte(0x4f,0x4D);
	HDMI_WriteI2C_Byte(0x50,0xF3);
	HDMI_WriteI2C_Byte(0x51,0x80);
	HDMI_WriteI2C_Byte(0x1f,0x90);
	HDMI_WriteI2C_Byte(0x20,0x01);
	HDMI_WriteI2C_Byte(0x21,0x68);
	HDMI_WriteI2C_Byte(0x22,0x01);
	HDMI_WriteI2C_Byte(0x23,0x5E);
	HDMI_WriteI2C_Byte(0x24,0x01);
	HDMI_WriteI2C_Byte(0x25,0x54);
	HDMI_WriteI2C_Byte(0x26,0x01);
	HDMI_WriteI2C_Byte(0x27,0x90);
	HDMI_WriteI2C_Byte(0x28,0x01);
	HDMI_WriteI2C_Byte(0x29,0x68);
	HDMI_WriteI2C_Byte(0x2a,0x01);
	HDMI_WriteI2C_Byte(0x2b,0x5E);
	HDMI_WriteI2C_Byte(0x2c,0x01);
	HDMI_WriteI2C_Byte(0x2d,0x54);
	HDMI_WriteI2C_Byte(0x2e,0x01);
	HDMI_WriteI2C_Byte(0x42,0x64);
	HDMI_WriteI2C_Byte(0x43,0x00);
	HDMI_WriteI2C_Byte(0x44,0x04);
	HDMI_WriteI2C_Byte(0x45,0x00);
	HDMI_WriteI2C_Byte(0x46,0x59);
	HDMI_WriteI2C_Byte(0x47,0x00);
	HDMI_WriteI2C_Byte(0x48,0xf2);
	HDMI_WriteI2C_Byte(0x49,0x06);
	HDMI_WriteI2C_Byte(0x4a,0x00);
	HDMI_WriteI2C_Byte(0x4b,0x72);
	HDMI_WriteI2C_Byte(0x4c,0x45);
	HDMI_WriteI2C_Byte(0x4d,0x00);
	HDMI_WriteI2C_Byte(0x52,0x08);
	HDMI_WriteI2C_Byte(0x53,0x00);
	HDMI_WriteI2C_Byte(0x54,0xb2);
	HDMI_WriteI2C_Byte(0x55,0x00);
	HDMI_WriteI2C_Byte(0x56,0xe4);
	HDMI_WriteI2C_Byte(0x57,0x0d);
	HDMI_WriteI2C_Byte(0x58,0x00);
	HDMI_WriteI2C_Byte(0x59,0xe4);
	HDMI_WriteI2C_Byte(0x5a,0x8a);
	HDMI_WriteI2C_Byte(0x5b,0x00);
	HDMI_WriteI2C_Byte(0x5c,0x34);
	HDMI_WriteI2C_Byte(0x1e,0x4f);
	HDMI_WriteI2C_Byte(0x51,0x00);
	
	
	//	AudioIIsEn();
		my_mipi_i2c->mdss_mipi_i2c_client->addr = 0x48;
		HDMI_WriteI2C_Byte(0xB2,0x01);
		my_mipi_i2c->mdss_mipi_i2c_client->addr = 0x4a;
	HDMI_WriteI2C_Byte(0x06,0x08);
	HDMI_WriteI2C_Byte(0x07,0xF0);
	HDMI_WriteI2C_Byte(0x34,0xD2);
		
	
	
	//	MIPIRxLogicRes();
		my_mipi_i2c->mdss_mipi_i2c_client->addr = 0x48;
		HDMI_WriteI2C_Byte(0x03,0x7f);
		mdelay(100);
		HDMI_WriteI2C_Byte(0x03,0xff);
		
#ifdef _LVDS_Output_

//	Coll PLL寄存器设置
//	void Collpll51M(void)// PLL 51.25MHz

	Core_PLL_Ratio.Temp32 = (Panel_Pixel_CLK/25)*7;

	my_mipi_i2c->mdss_mipi_i2c_client->addr = 0x48;
	HDMI_WriteI2C_Byte(0x50,0x24);
	HDMI_WriteI2C_Byte(0x51,0x05);
	HDMI_WriteI2C_Byte(0x52,0x14);

	HDMI_WriteI2C_Byte(0x69,(u8)((Core_PLL_Ratio.Temp32/100)&0x000000FF));
	HDMI_WriteI2C_Byte(0x69,0x80+(u8)((Core_PLL_Ratio.Temp32/100)&0x000000FF));

	Core_PLL_Ratio.Temp32 = (Core_PLL_Ratio.Temp32 % 100)&0x000000FF;
	Core_PLL_Ratio.Temp32 = Core_PLL_Ratio.Temp32*16384;
	Core_PLL_Ratio.Temp32 = Core_PLL_Ratio.Temp32/100;
//***********************************************************//
	// 注意大小端，此处为大端模式（Big-endian） 
	//HDMI_WriteI2C_Byte(0x6c,0x80 + Core_PLL_Ratio.Temp8[2]);
	//HDMI_WriteI2C_Byte(0x6b,Core_PLL_Ratio.Temp8[3]);

	// 注意大小端，此处为小端模式（Little-endian） 
	HDMI_WriteI2C_Byte(0x6c,0x80 + Core_PLL_Ratio.Temp8[1]);
	HDMI_WriteI2C_Byte(0x6b,Core_PLL_Ratio.Temp8[0]);
//***********************************************************//
//------------------------------------------//

//	LVDS Output寄存器设置
//	void LVDS_Scale_Ratio(void)

my_mipi_i2c->mdss_mipi_i2c_client->addr = 0x48;
	HDMI_WriteI2C_Byte(0x80,0x00);
	HDMI_WriteI2C_Byte(0x81,0xff);
	HDMI_WriteI2C_Byte(0x82,0x03);

	HDMI_WriteI2C_Byte(0x83,(u8)(MIPI_Timing[H_act]%256));
	HDMI_WriteI2C_Byte(0x84,(u8)(MIPI_Timing[H_act]/256));

	HDMI_WriteI2C_Byte(0x85,0x80);
	HDMI_WriteI2C_Byte(0x86,0x10);

	HDMI_WriteI2C_Byte(0x87,(u8)(LVDS_Panel_Timing[H_tol]%256));
	HDMI_WriteI2C_Byte(0x88,(u8)(LVDS_Panel_Timing[H_tol]/256));
	HDMI_WriteI2C_Byte(0x89,(u8)(LVDS_Panel_Timing[H_sync]%256));
	HDMI_WriteI2C_Byte(0x8a,(u8)(LVDS_Panel_Timing[H_bp]%256));
	HDMI_WriteI2C_Byte(0x8b,(u8)((LVDS_Panel_Timing[H_bp]/256)*0x80 + (LVDS_Panel_Timing[V_sync]%256)));
	HDMI_WriteI2C_Byte(0x8c,(u8)(LVDS_Panel_Timing[H_act]%256));
	HDMI_WriteI2C_Byte(0x8d,(u8)(LVDS_Panel_Timing[V_act]%256));
	HDMI_WriteI2C_Byte(0x8e,(u8)((LVDS_Panel_Timing[V_act]/256)*0x10 + (LVDS_Panel_Timing[H_act]/256)));

	f_DIV = (((float)(MIPI_Timing[H_act]-1))/(float)(LVDS_Panel_Timing[H_act]-1))*4096;
	Core_PLL_Ratio.Temp32 = (u32)f_DIV;
//***********************************************************//
	// 注意大小端，此处为大端模式（Big-endian） 
	//HDMI_WriteI2C_Byte(0x8f,Core_PLL_Ratio.Temp8[3]);
	//HDMI_WriteI2C_Byte(0x90,Core_PLL_Ratio.Temp8[2]);

	// 注意大小端，此处为小端模式（Little-endian）
	HDMI_WriteI2C_Byte(0x8f,Core_PLL_Ratio.Temp8[0]);
	HDMI_WriteI2C_Byte(0x90,Core_PLL_Ratio.Temp8[1]);
//***********************************************************//

	f_DIV = (((float)(MIPI_Timing[V_act]-1))/(float)(LVDS_Panel_Timing[V_act]-1))*4096;
	Core_PLL_Ratio.Temp32 = (u32)f_DIV;
//***********************************************************//
	// 注意大小端，此处为大端模式（Big-endian） 
	//HDMI_WriteI2C_Byte(0x91,Core_PLL_Ratio.Temp8[3]);
	//HDMI_WriteI2C_Byte(0x92,Core_PLL_Ratio.Temp8[2]);

	// 注意大小端，此处为小端模式（Little-endian） 
	HDMI_WriteI2C_Byte(0x91,Core_PLL_Ratio.Temp8[1]);
	HDMI_WriteI2C_Byte(0x92,Core_PLL_Ratio.Temp8[0]);
//***********************************************************//
	HDMI_WriteI2C_Byte(0x7f,0x9c);

#ifdef _8bit_
	HDMI_WriteI2C_Byte(0xa8,0x1b);// 单8 LVDS
#else
	HDMI_WriteI2C_Byte(0xa8,0x1f);// 单6 LVDS
#endif

	mdelay(3000);

//	void LvdsPowerUp(void)
	my_mipi_i2c->mdss_mipi_i2c_client->addr = 0x48;
	HDMI_WriteI2C_Byte(0x44,0x30);
#endif

#ifdef TEST_PATTERN_MODE

	Test_pattern_1280x720();

#endif

	mutex_unlock(&my_mipi_i2c->i2c_lock);

	printk("eliot :mdss_mipi_i2c_init end \n");
	return 0 ;
	
}

EXPORT_SYMBOL_GPL(mdss_mipi_i2c_init);

static int lt_pinctrl_init(struct mdss_i2c_interface *lt)
{
	struct i2c_client *client = lt->mdss_mipi_i2c_client;

	lt->pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR_OR_NULL(lt->pinctrl)) {
		dev_err(&client->dev, "Failed to get pinctrl\n");
		return PTR_ERR(lt->pinctrl);
	}

	lt->pin_default = pinctrl_lookup_state(lt->pinctrl, "default");
	if (IS_ERR_OR_NULL(lt->pin_default)) {
		dev_err(&client->dev, "Failed to look up default state\n");
		return PTR_ERR(lt->pin_default);
	}

	lt->pin_sleep = pinctrl_lookup_state(lt->pinctrl, "sleep");
	if (IS_ERR_OR_NULL(lt->pin_sleep)) {
		dev_err(&client->dev, "Failed to look up sleep state\n");
		return PTR_ERR(lt->pin_sleep);
	}

	return 0;
}

/*
	1.get reset pin and request gpio ;
	2.get i2c client ,check i2c function and test read i2c slave ;
	3.send lt8912 i2c configuration table
*/
static int mipi_i2c_probe(struct i2c_client *client,
        const struct i2c_device_id *id) 
{
    int err;
	struct mdss_dsi_ctrl_pdata *ctrl=NULL;
	int from_mdp=0 ;

	printk("eliot :mipi_i2c_probe start.....");
	
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev,
				"%s: check_functionality failed.", __func__);
		err = -ENODEV;
		goto exit0;
	}

	/* Allocate memory for driver data */
	my_mipi_i2c = kzalloc(sizeof(struct mdss_i2c_interface), GFP_KERNEL);
	if (!my_mipi_i2c) {
		dev_err(&client->dev,
				"%s: memory allocation failed.", __func__);
		err = -ENOMEM;
		goto exit1;
	}
	
	mutex_init(&my_mipi_i2c->lock);
	mutex_init(&my_mipi_i2c->i2c_lock);

	if (client->dev.of_node) {
	my_mipi_i2c->gpio_rstn = of_get_named_gpio_flags(client->dev.of_node,
			"lt,gpio_rstn", 0, NULL);
	//printk("eliot:my_mipi_i2c->gpio_rstn=%d\n",my_mipi_i2c->gpio_rstn);
	}
	else
	{
		printk("eliot:client->dev.of_node not exit!\n");
	}

	/***** I2C initialization *****/
	my_mipi_i2c->mdss_mipi_i2c_client = client;
	
	/* set client data */
	i2c_set_clientdata(client, my_mipi_i2c);

	/* initialize pinctrl */
	if (!lt_pinctrl_init(my_mipi_i2c)) {
		err = pinctrl_select_state(my_mipi_i2c->pinctrl, my_mipi_i2c->pin_default);
		if (err) {
			dev_err(&client->dev, "Can't select pinctrl state\n");
			goto exit2;
		}
	}
	mutex_lock(&my_mipi_i2c->lock);

	/* Pull up the reset pin */
	/* request  GPIO  */
	err = gpio_request(my_mipi_i2c->gpio_rstn, "lt8912_rsrn");
	if (err < 0) {
		mutex_unlock(&my_mipi_i2c->lock);
		printk("Failed to request GPIO:%d, ERRNO:%d",
			  my_mipi_i2c->gpio_rstn, err);
		err = -ENODEV;
	}else{
		gpio_direction_output(my_mipi_i2c->gpio_rstn, 0);
		mdelay(250);
		gpio_direction_output(my_mipi_i2c->gpio_rstn, 1);

		}
	my_mipi_i2c->mdss_mipi_i2c_client->addr = 0x48;
    err = i2c_smbus_read_byte_data(client, 0x08);
    if (err < 0) {
		I2C_TEST_OK = 1 ;
		mutex_unlock(&my_mipi_i2c->lock);
		gpio_direction_output(my_mipi_i2c->gpio_rstn, 0);
		dev_err(&client->dev, "i2c read fail: can't read from %02x: %d\n", 0, err);
        goto exit3;
    } 
	mdss_mipi_i2c_init(ctrl,from_mdp);
	//test_read();
	mutex_unlock(&my_mipi_i2c->lock);
	printk("eliot :mipi_i2c_probe end ....\n");
	return 0;
exit3:
	gpio_free(my_mipi_i2c->gpio_rstn);
exit2:
	kfree(my_mipi_i2c);
exit1:
exit0:
	return err;

}

static const struct of_device_id mipi_i2c_of_match[] = {
    { .compatible = "qcom,lt8912",},
    {},
};

static const struct i2c_device_id mipi_i2c_id[] = {
    {"qcom,lt8912", 0},
    {},
};
static int mipi_i2c_resume(struct device *tdev) {
	Reset_chip();
    return 0;
}

static int mipi_i2c_remove(struct i2c_client *client) {
	
    return 0;
}

static int mipi_i2c_suspend(struct device *tdev) {
	gpio_direction_output(my_mipi_i2c->gpio_rstn, 0);
    return 0;
}

static const struct dev_pm_ops mipi_i2c_pm_ops =
{ 
    .suspend = mipi_i2c_suspend,
    .resume = mipi_i2c_resume, 
};


static struct i2c_driver mipi_i2c_driver = {
    .driver = {
        .name = "qcom,lt8912",
        .owner    = THIS_MODULE,
        .of_match_table = mipi_i2c_of_match,
        .pm = &mipi_i2c_pm_ops,
    },
    .probe    = mipi_i2c_probe,
    .remove   = mipi_i2c_remove,
    .id_table = mipi_i2c_id,
};

module_i2c_driver(mipi_i2c_driver);

MODULE_LICENSE("GPL");

