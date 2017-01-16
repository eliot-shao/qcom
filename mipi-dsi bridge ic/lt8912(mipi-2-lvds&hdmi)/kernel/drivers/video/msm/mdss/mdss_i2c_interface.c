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
	gpio_direction_output(1018, 1);
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

static void Test_pattern_720x480(void)
{
	my_mipi_i2c->mdss_mipi_i2c_client->addr = 0x49;
	HDMI_WriteI2C_Byte(0x72,0x12);
	HDMI_WriteI2C_Byte(0x73,0x7a);//RGD_PTN_DE_DLY[7:0]
	HDMI_WriteI2C_Byte(0x74,0x00);//RGD_PTN_DE_DLY[11:8]  192
	HDMI_WriteI2C_Byte(0x75,0x24);//RGD_PTN_DE_TOP[6:0]  41
	HDMI_WriteI2C_Byte(0x76,0xd0);//RGD_PTN_DE_CNT[7:0]
	HDMI_WriteI2C_Byte(0x77,0xe0);//RGD_PTN_DE_LIN[7:0]
	HDMI_WriteI2C_Byte(0x78,0x12);//RGD_PTN_DE_LIN[10:8],RGD_PTN_DE_CNT[11:8]
	HDMI_WriteI2C_Byte(0x79,0x5a);//RGD_PTN_H_TOTAL[7:0]
	HDMI_WriteI2C_Byte(0x7a,0x0d);//RGD_PTN_V_TOTAL[7:0]
	HDMI_WriteI2C_Byte(0x7b,0x23);//RGD_PTN_V_TOTAL[10:8],RGD_PTN_H_TOTAL[11:8]
	HDMI_WriteI2C_Byte(0x7c,0x3e);//RGD_PTN_HWIDTH[7:0]
	HDMI_WriteI2C_Byte(0x7d,0x06);//RGD_PTN_HWIDTH[9:8],RGD_PTN_VWIDTH[5:0]

	HDMI_WriteI2C_Byte(0x70,0x80);
	HDMI_WriteI2C_Byte(0x71,0x17);

	HDMI_WriteI2C_Byte(0x4e,0x66);
	HDMI_WriteI2C_Byte(0x4f,0x66);
	HDMI_WriteI2C_Byte(0x50,0x26);
	HDMI_WriteI2C_Byte(0x51,0x80);
}
static void Test_pattern_640x350(void)
{
	my_mipi_i2c->mdss_mipi_i2c_client->addr = 0x49;
	HDMI_WriteI2C_Byte(0x72,0x12);
	HDMI_WriteI2C_Byte(0x73,0xa0);//RGD_PTN_DE_DLY[7:0]
	HDMI_WriteI2C_Byte(0x74,0x00);//RGD_PTN_DE_DLY[11:8]  192
	HDMI_WriteI2C_Byte(0x75,0x3f);//RGD_PTN_DE_TOP[6:0]  41
	HDMI_WriteI2C_Byte(0x76,0x80);//RGD_PTN_DE_CNT[7:0]
	HDMI_WriteI2C_Byte(0x77,0x5e);//RGD_PTN_DE_LIN[7:0]
	HDMI_WriteI2C_Byte(0x78,0x12);//RGD_PTN_DE_LIN[10:8],RGD_PTN_DE_CNT[11:8]
	HDMI_WriteI2C_Byte(0x79,0x40);//RGD_PTN_H_TOTAL[7:0]
	HDMI_WriteI2C_Byte(0x7a,0xbd);//RGD_PTN_V_TOTAL[7:0]
	HDMI_WriteI2C_Byte(0x7b,0x13);//RGD_PTN_V_TOTAL[10:8],RGD_PTN_H_TOTAL[11:8]
	HDMI_WriteI2C_Byte(0x7c,0x40);//RGD_PTN_HWIDTH[7:0]
	HDMI_WriteI2C_Byte(0x7d,0x3);//RGD_PTN_HWIDTH[9:8],RGD_PTN_VWIDTH[5:0]

	HDMI_WriteI2C_Byte(0x70,0x80);
	HDMI_WriteI2C_Byte(0x71,0x17);

	HDMI_WriteI2C_Byte(0x4e,0x66);
	HDMI_WriteI2C_Byte(0x4f,0x66);
	HDMI_WriteI2C_Byte(0x50,0x26);
	HDMI_WriteI2C_Byte(0x51,0x80);
}
static void Test_pattern_1280x720(void)
{
	my_mipi_i2c->mdss_mipi_i2c_client->addr = 0x49;
	HDMI_WriteI2C_Byte(0x72,0x12);
	HDMI_WriteI2C_Byte(0x73,0x04);//RGD_PTN_DE_DLY[7:0]
	HDMI_WriteI2C_Byte(0x74,0x01);//RGD_PTN_DE_DLY[11:8]  192
	HDMI_WriteI2C_Byte(0x75,0x19);//RGD_PTN_DE_TOP[6:0]  41
	HDMI_WriteI2C_Byte(0x76,0x00);//RGD_PTN_DE_CNT[7:0]
	HDMI_WriteI2C_Byte(0x77,0xD0);//RGD_PTN_DE_LIN[7:0]
	HDMI_WriteI2C_Byte(0x78,0x25);//RGD_PTN_DE_LIN[10:8],RGD_PTN_DE_CNT[11:8]
	HDMI_WriteI2C_Byte(0x79,0x72);//RGD_PTN_H_TOTAL[7:0]
	HDMI_WriteI2C_Byte(0x7a,0xEE);//RGD_PTN_V_TOTAL[7:0]
	HDMI_WriteI2C_Byte(0x7b,0x26);//RGD_PTN_V_TOTAL[10:8],RGD_PTN_H_TOTAL[11:8]
	HDMI_WriteI2C_Byte(0x7c,0x28);//RGD_PTN_HWIDTH[7:0]
	HDMI_WriteI2C_Byte(0x7d,0x05);//RGD_PTN_HWIDTH[9:8],RGD_PTN_VWIDTH[5:0]

	HDMI_WriteI2C_Byte(0x70,0x80);
	HDMI_WriteI2C_Byte(0x71,0x17);

	HDMI_WriteI2C_Byte(0x4e,0x93);
	HDMI_WriteI2C_Byte(0x4f,0x3E);
	HDMI_WriteI2C_Byte(0x50,0x69);
	HDMI_WriteI2C_Byte(0x51,0x80);
}

#endif

/*This function para Get from FAE*/
int mdss_mipi_i2c_init(struct mdss_dsi_ctrl_pdata *ctrl, int from_mdp)
{
	printk("eliot :mdss_mipi_i2c_init start add lvds \n");
	//Reset_chip();
	/*if I2C_TEST_OK == 1 stand for lt8912's i2c no ack , and the 
	resource of my_mipi_i2c has been remove .must be stop here !
	*/
	if(I2C_TEST_OK  == 1)
		return 0;
	// 往LT8912寄存器写值：
	//******************************************//
		mutex_lock(&my_mipi_i2c->i2c_lock);
	//	DigitalClockEn();
		my_mipi_i2c->mdss_mipi_i2c_client->addr = 0x48; // IIC address
		HDMI_WriteI2C_Byte(0x08,0xff);// Register address : 0x08;	Value : 0xff
		HDMI_WriteI2C_Byte(0x09,0xff);
		HDMI_WriteI2C_Byte(0x0a,0xff);
		HDMI_WriteI2C_Byte(0x0b,0xff);
		HDMI_WriteI2C_Byte(0x0c,0xff);
	
	//******************************************//
	
	//	TxAnalog();
		my_mipi_i2c->mdss_mipi_i2c_client->addr = 0x48;
		HDMI_WriteI2C_Byte(0x31,0xa1);
		HDMI_WriteI2C_Byte(0x32,0xa1);
		HDMI_WriteI2C_Byte(0x33,0x03);
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
		HDMI_WriteI2C_Byte(0x10,0x20);	
		HDMI_WriteI2C_Byte(0x11,0x04);	
		HDMI_WriteI2C_Byte(0x12,0x04);	
		HDMI_WriteI2C_Byte(0x13,0x00);	// 00 4 lane  // 01 lane // 02 2 lane //03 3 lane
		HDMI_WriteI2C_Byte(0x14,0x00);	
		HDMI_WriteI2C_Byte(0x15,0x00);
		HDMI_WriteI2C_Byte(0x1a,0x03);	
		HDMI_WriteI2C_Byte(0x1b,0x03);	
		
	//******************************************//
	
	//	void LvdsPowerDown(void)
		my_mipi_i2c->mdss_mipi_i2c_client->addr = 0x48;
		HDMI_WriteI2C_Byte(0x44,0x31);
	
	//******************************************//
	
	//	MIPIDig1280x720();
		my_mipi_i2c->mdss_mipi_i2c_client->addr = 0x49;
		HDMI_WriteI2C_Byte(0x18,0x28); // hwidth
		HDMI_WriteI2C_Byte(0x19,0x05); // vwidth 6
		HDMI_WriteI2C_Byte(0x1c,0x00); // H_active[7:0]
		HDMI_WriteI2C_Byte(0x1d,0x05); // H_active[15:8]
		HDMI_WriteI2C_Byte(0x1e,0x67); // hs/vs/de pol hdmi sel pll sel
		HDMI_WriteI2C_Byte(0x2f,0x0c); // fifo_buff_length 12
		HDMI_WriteI2C_Byte(0x34,0x72); // H_total[7:0]
		HDMI_WriteI2C_Byte(0x35,0x06); // H_total[15:8]
		HDMI_WriteI2C_Byte(0x36,0xEE); // V_total[7:0]
		HDMI_WriteI2C_Byte(0x37,0x02); // V_total[15:8]
		HDMI_WriteI2C_Byte(0x38,0x14); // VBP[7:0]
		HDMI_WriteI2C_Byte(0x39,0x00); // VBP[15:8]
		HDMI_WriteI2C_Byte(0x3a,0x05); // VFP[7:0]
		HDMI_WriteI2C_Byte(0x3b,0x00); // VFP[15:8]
		HDMI_WriteI2C_Byte(0x3c,0xDC); // HBP[7:0]
		HDMI_WriteI2C_Byte(0x3d,0x00); // HBP[15:8]
		HDMI_WriteI2C_Byte(0x3e,0x6E); // HFP[7:0]
		HDMI_WriteI2C_Byte(0x3f,0x00); // HFP[15:8]



	
	//******************************************//
	
	//	DDSConfig();
		my_mipi_i2c->mdss_mipi_i2c_client->addr = 0x49;
		HDMI_WriteI2C_Byte(0x4e,0x93);
		HDMI_WriteI2C_Byte(0x4f,0x3E);
		HDMI_WriteI2C_Byte(0x50,0x69);

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
	
	//******************************************//
	
	//	AudioIIsEn();
		my_mipi_i2c->mdss_mipi_i2c_client->addr = 0x48;
		HDMI_WriteI2C_Byte(0xB2,0x01);
		my_mipi_i2c->mdss_mipi_i2c_client->addr = 0x4a;
		HDMI_WriteI2C_Byte(0x06,0x08);
		HDMI_WriteI2C_Byte(0x07,0xF0);
		HDMI_WriteI2C_Byte(0x34,0xD2);
		
	
	//******************************************//
	
	//	MIPIRxLogicRes();
		my_mipi_i2c->mdss_mipi_i2c_client->addr = 0x48;
		HDMI_WriteI2C_Byte(0x03,0x7f);
		mdelay(100);
		HDMI_WriteI2C_Byte(0x03,0xff);
	
	//******************************************//

	//	void Collpll51M(void)// PLL 80.4MHz
		my_mipi_i2c->mdss_mipi_i2c_client->addr = 0x48;
		HDMI_WriteI2C_Byte(0x50,0x24);
		HDMI_WriteI2C_Byte(0x51,0x05);
		HDMI_WriteI2C_Byte(0x52,0x14);
		HDMI_WriteI2C_Byte(0x69,0x16);
		HDMI_WriteI2C_Byte(0x69,0x96);
		HDMI_WriteI2C_Byte(0x6c,0xa0);
		HDMI_WriteI2C_Byte(0x6b,0xc4);

	//******************************************//

	//	1366X768
		my_mipi_i2c->mdss_mipi_i2c_client->addr = 0x48;
		HDMI_WriteI2C_Byte(0x80,0x00);
		HDMI_WriteI2C_Byte(0x81,0xff);
		HDMI_WriteI2C_Byte(0x82,0x03);
		HDMI_WriteI2C_Byte(0x83,0x00);
		HDMI_WriteI2C_Byte(0x84,0x05);
		HDMI_WriteI2C_Byte(0x85,0x80);
		HDMI_WriteI2C_Byte(0x86,0x10);
		HDMI_WriteI2C_Byte(0x87,0x40);
		HDMI_WriteI2C_Byte(0x88,0x06);
		HDMI_WriteI2C_Byte(0x89,0x5b);
		HDMI_WriteI2C_Byte(0x8a,0x63);
		HDMI_WriteI2C_Byte(0x8b,0x05);
		HDMI_WriteI2C_Byte(0x8c,0x56);
		HDMI_WriteI2C_Byte(0x8d,0x0);
		HDMI_WriteI2C_Byte(0x8e,0x35);
		HDMI_WriteI2C_Byte(0x8f,0xfe);
		HDMI_WriteI2C_Byte(0x90,0x0e);
		HDMI_WriteI2C_Byte(0x91,0x0);
		HDMI_WriteI2C_Byte(0x92,0x0f);
		HDMI_WriteI2C_Byte(0x7f,0x9c);
		HDMI_WriteI2C_Byte(0xa8,0x1b);

		mdelay(3000);

	//	void LvdsPowerUp(void)
		my_mipi_i2c->mdss_mipi_i2c_client->addr = 0x48;
		HDMI_WriteI2C_Byte(0x44,0x30);

//******************************************//
	
#ifdef TEST_PATTERN_MODE

	Test_pattern_1280x720();

#endif

	mutex_unlock(&my_mipi_i2c->i2c_lock);

	printk("eliot :mdss_mipi_i2c_init end OK\n");
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
		//mutex_unlock(&my_mipi_i2c->lock);
		printk("Failed to request GPIO:%d, ERRNO:%d",
			  my_mipi_i2c->gpio_rstn, err);
		err = -ENODEV;
	}else{
		gpio_direction_output(my_mipi_i2c->gpio_rstn, 0);
		mdelay(250);
		gpio_direction_output(my_mipi_i2c->gpio_rstn, 1);

		}
	err = gpio_request(1018, "LVDS_PANEL_EN");
	if (err < 0) {
		printk("Failed to request GPIO:116\n");
		err = -ENODEV;
	}else{
		gpio_direction_output(1018, 1);
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

