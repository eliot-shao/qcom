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
	int gpio_rstn ;//for icn6211
	int gpio_3V3_en;
	int gpio_dc_5v_en;
	struct pinctrl		*pinctrl;
	struct pinctrl_state	*pin_default;
	struct pinctrl_state	*pin_sleep;
	
	struct regulator *vdd;
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

static int Reset_chip(void)
{

	gpio_direction_output(my_mipi_i2c->gpio_rstn, 0);
	mdelay(50);

	gpio_direction_output(my_mipi_i2c->gpio_rstn, 1);

	return 0;
}
/* no external clk 
0x20 , 0x00
0x21 , 0xD0
0x22 , 0x25
0x23 , 0x6E
0x24 , 0x28
0x25 , 0xDC
0x26 , 0x00
0x27 , 0x05
0x28 , 0x05
0x29 , 0x14
0x34 , 0x80
0x36 , 0x6E
0xB5 , 0xA0
0x5C , 0xFF
0x2A , 0x07
0x56 , 0x92
0x6B , 0x52
0x69 , 0x2C
0x10 , 0x40
0x11 , 0x88
0xB6 , 0x20
0x51 , 0x20
0x09 , 0x10
*/
/*own external clk
0x20 = 0x00
0x21 = 0xD0
0x22 = 0x25
0x23 = 0x6E
0x24 = 0x28
0x25 = 0xDC
0x26 = 0x00
0x27 = 0x05
0x28 = 0x05
0x29 = 0x14
0x34 = 0x80
0x36 = 0x6E
0xB5 = 0xA0
0x5C = 0xFF
0x2A = 0x07
0x56 = 0x90
0x6B = 0x51
0x69 = 0x2F
0x10 = 0x40
0x11 = 0x88
0xB6 = 0x20
0x51 = 0x20
0x09 = 0x10
*/
/*This function para Get from FAE*/
int mdss_mipi_i2c_init(struct mdss_dsi_ctrl_pdata *ctrl, int from_mdp)
{
	//printk(KERN_ERR "eliot :mdss_mipi_i2c_init start \n");
	//Reset_chip();
	/*if I2C_TEST_OK == 1 stand for lt8912's i2c no ack , and the 
	resource of my_mipi_i2c has been remove .must be stop here !
	*/
	if(I2C_TEST_OK  == 0)
	{
		printk(KERN_ERR "eliot :i2c test erro or mdss i2c interface donot probe \n");
		return 0;
	}
	// ÍùLT8912¼Ä´æÆ÷Ð´Öµ£º
	//******************************************//
		mutex_lock(&my_mipi_i2c->i2c_lock);

		HDMI_WriteI2C_Byte(0x20 , 0x00);// Register address : 0x08;	Value : 0xff
		HDMI_WriteI2C_Byte(0x21 , 0xD0);
		HDMI_WriteI2C_Byte(0x22 , 0x25);
		HDMI_WriteI2C_Byte(0x23 , 0x6E);
		HDMI_WriteI2C_Byte(0x24 , 0x28);
		HDMI_WriteI2C_Byte(0x25 , 0xDC);
		HDMI_WriteI2C_Byte(0x26 , 0x00);
		HDMI_WriteI2C_Byte(0x27 , 0x05);
		HDMI_WriteI2C_Byte(0x28 , 0x05);
		HDMI_WriteI2C_Byte(0x29 , 0x14);
		HDMI_WriteI2C_Byte(0x34 , 0x80);
		HDMI_WriteI2C_Byte(0x36 , 0x6E);
		HDMI_WriteI2C_Byte(0xB5 , 0xA0);
		HDMI_WriteI2C_Byte(0x5C , 0xFF);
		HDMI_WriteI2C_Byte(0x2A , 0x07);
		HDMI_WriteI2C_Byte(0x56 , 0x92);
		HDMI_WriteI2C_Byte(0x6B , 0x52);
		HDMI_WriteI2C_Byte(0x69 , 0x2C);
		HDMI_WriteI2C_Byte(0x10 , 0x40);
		HDMI_WriteI2C_Byte(0x11 , 0x88);
		HDMI_WriteI2C_Byte(0xB6 , 0x20);
		HDMI_WriteI2C_Byte(0x51 , 0x20);
		HDMI_WriteI2C_Byte(0x09 , 0x10);
		
		mutex_unlock(&my_mipi_i2c->i2c_lock);

		//printk(KERN_ERR "eliot :mdss_mipi_i2c_init end \n");
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
#define I2C_VTG_MIN_UV	1800000
#define I2C_VTG_MAX_UV	1800000

static int mipi_i2c_probe(struct i2c_client *client,
        const struct i2c_device_id *id) 
{
  int err;
	struct mdss_dsi_ctrl_pdata *ctrl=NULL;
	int from_mdp=0 ;

	printk(KERN_ERR "eliot :mipi_i2c_probe start.....");
	
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
			"icn,gpio_rstn", 0, NULL);
	my_mipi_i2c->gpio_3V3_en= of_get_named_gpio_flags(client->dev.of_node,
			"icn,gpio_3V3_en", 0, NULL);
	my_mipi_i2c->gpio_dc_5v_en= of_get_named_gpio_flags(client->dev.of_node,
			"icn,gpio_dc_5v_en", 0, NULL);
	//printk("eliot:my_mipi_i2c->gpio_rstn=%d\n",my_mipi_i2c->gpio_rstn);
	}
	else
	{
		printk(KERN_ERR "eliot:client->dev.of_node not exit!\n");
	}

	/***** I2C initialization *****/
	my_mipi_i2c->mdss_mipi_i2c_client = client;
	
	/* set client data */
	i2c_set_clientdata(client, my_mipi_i2c);

		/*donot free power , camera using else*/
	err = gpio_request(my_mipi_i2c->gpio_3V3_en, "gpio_3V3_en");
	err = gpio_request(my_mipi_i2c->gpio_dc_5v_en, "gpio_dc_5v_en");
	gpio_direction_output(my_mipi_i2c->gpio_3V3_en, 1);
	gpio_direction_output(my_mipi_i2c->gpio_dc_5v_en, 1);
	
	/*open l6 1.8 V*/
	
	my_mipi_i2c->vdd = regulator_get(&client->dev, "vdd");
	if (IS_ERR(my_mipi_i2c->vdd)) {
		err = PTR_ERR(my_mipi_i2c->vdd);
		printk(KERN_ERR "eliot:vdd get erro err = %d\n",err);
		goto exit1;
	}else
	{
		regulator_set_voltage(my_mipi_i2c->vdd, I2C_VTG_MIN_UV,
					   I2C_VTG_MAX_UV);
		err = regulator_enable(my_mipi_i2c->vdd);
		if (err) {
			dev_err(&client->dev,
				"Regulator vdd enable failed ret=%d\n", err);
			goto exit2;
		}
	}

	
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
	err = gpio_request(my_mipi_i2c->gpio_rstn, "icn6211_rsrn");
	if (err < 0) {
		mutex_unlock(&my_mipi_i2c->lock);
		printk(KERN_ERR "Failed to request GPIO:%d, ERRNO:%d",
			  my_mipi_i2c->gpio_rstn, err);
		err = -ENODEV;
		goto exit2;
	}else{
			gpio_direction_output(my_mipi_i2c->gpio_rstn, 0);
			mdelay(50);
			gpio_direction_output(my_mipi_i2c->gpio_rstn, 1);
		}
		//printk(KERN_ERR "eliot :icn6211 addr = 0x%x ....\n",my_mipi_i2c->mdss_mipi_i2c_client->addr);
    err = i2c_smbus_read_byte_data(client, 0x0);
    if ((err < 0)&&(err != 0xc1)) {
			//I2C_TEST_OK = 0 ;
			mutex_unlock(&my_mipi_i2c->lock);
			gpio_direction_output(my_mipi_i2c->gpio_rstn, 0);
			dev_err(&client->dev, "i2c read fail: can't read from %02x: %d\n", client->addr, err);
	        goto exit3;
    } 
	I2C_TEST_OK = 1 ;
  //printk(KERN_ERR "eliot :icn6211 reg 0 val = 0x%x ....\n",err);
	mdss_mipi_i2c_init(ctrl,from_mdp);
	mutex_unlock(&my_mipi_i2c->lock);
	printk(KERN_ERR "eliot :mipi_i2c_probe end ....\n");
	return 0;
exit3:
	gpio_free(my_mipi_i2c->gpio_rstn);

exit2:
	regulator_disable(my_mipi_i2c->vdd);
exit1:
	mutex_destroy(&my_mipi_i2c->lock);
	mutex_destroy(&my_mipi_i2c->i2c_lock);
	kfree(my_mipi_i2c);
exit0:
	return err;

}

static const struct of_device_id mipi_i2c_of_match[] = {
    { .compatible = "qcom,icn6211",},
    {},
};

static const struct i2c_device_id mipi_i2c_id[] = {
    {"qcom,icn6211", 0},
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
        .name = "qcom,icn6211",
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

