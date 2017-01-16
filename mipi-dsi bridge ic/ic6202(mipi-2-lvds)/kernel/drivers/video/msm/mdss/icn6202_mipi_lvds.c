/**
 * @file icn6202_mipi_lvds.c
 * @brief generate i2c_client, add an interface that other module can get the client
 * @author fangchengbing
 * @version 1.0
 * @date 2015-05-04
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

#define ICN6211_REG_0 (0xC1)
#define ICN6202_ADD	(0x2D)

struct icn6202_platform_data {
	int en_gpio;
};

struct i2c_client *icn6202_i2c_client = NULL;
static int icn6202_enable_gpio = -1;

/*other module can get i2c_client by the interface*/
struct i2c_client *mdss_get_icn6202_i2c_client(void)
{
	if (icn6202_i2c_client)
		return icn6202_i2c_client;

	return NULL;
}

EXPORT_SYMBOL_GPL(mdss_get_icn6202_i2c_client);

static int icn6202_set_enable(int enable_gpio)
{
	printk(KERN_ERR "[icn6202]: enable gpio number is %d\n", enable_gpio);

	if (!gpio_is_valid(enable_gpio)) {
		pr_err("[icn6202]: enable gpio is invaild\n");
		return -1;
	}

	if (gpio_request(enable_gpio, "ICN6202_ENABLE") < 0) {
		pr_err("[icn6202]: request enable gpio faild !\n");
		return -1;
	}

	if (gpio_direction_output(enable_gpio, 1) < 0) {
		pr_err("[icn6202]: set faild !\n");
		return -1;
	}

	icn6202_enable_gpio = enable_gpio;

	if (gpio_request(1018, "LCD_ENABLE_3V3") < 0) {
		pr_err("[icn6202]: request lcd enable 3v3 enable gpio faild !\n");
		return -1;
	}

	if (gpio_direction_output(1018, 1) < 0) {
		pr_err("[icn6202]: %d, set faild !\n",__LINE__);
		return -1;
	}

	return 0;
}

static int icn6202_parse_dt(struct device *dev,
		struct icn6202_platform_data *pdata)
{
	struct device_node *np = dev->of_node, *child;

	for_each_child_of_node(np, child) {
		enum of_gpio_flags flags;
		pdata->en_gpio = of_get_gpio_flags(child, 0, &flags);;
	}

	pr_err("[icn6202], %s,enable-gpio number is %d\n", 
			__func__,pdata->en_gpio);
	return 0;
}

static int icn6202_resume(struct device *tdev) 
{
	if (icn6202_enable_gpio > 0)
		gpio_direction_output(icn6202_enable_gpio, 1);
	return 0;
}

static int icn6202_remove(struct i2c_client *client) {
	return 0;
}

static int icn6202_suspend(struct device *tdev) 
{
	if (icn6202_enable_gpio > 0)
		gpio_direction_output(icn6202_enable_gpio, 0);
	return 0;
}

static int icn6202_probe(struct i2c_client *client,
		const struct i2c_device_id *id) {
	int ret;
	struct icn6202_platform_data *pdata;

	printk(KERN_ERR "[icn6202]: %s\n",__func__);

	if (client->addr != ICN6202_ADD) {
		printk(KERN_ERR "[icn6202]: Warning -- client->addr = 0x%02X\n",client->addr);
		return -1;
	}

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
				sizeof(struct icn6202_platform_data),
				GFP_KERNEL);
		if (!pdata) {
			pr_err("[icn6202]: Warning --Failed to allocate memory for pdata\n");
			return -ENOMEM;
		}

		if (icn6202_parse_dt(&client->dev, pdata)) {
			pr_err("[icn6202]: Warning -- parse dt faild!\n");
			return -1;
		}
	} else {
		pdata = client->dev.platform_data;
	}

	if (!pdata) {
		dev_err(&client->dev, "[icn6202]: ICN6202 invalid pdata\n");
		return -EINVAL;
	}

	/*set icn6202 enable gpio*/
	if(icn6202_set_enable(pdata->en_gpio)) {
		pr_err("[icn6202]: Waring, Enable icn6202 faild !\n");
		return -1;
	}

	if (!i2c_check_functionality(client->adapter,
				I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&client->dev, "SMBUS Byte Data not Supported\n");
		return -EIO;
	}

	ret = i2c_smbus_read_byte_data(client, 0);
	if (ret < 0) {
		dev_err(&client->dev, "[icn6202] i2c read fail: can't read from %02x: %d\n", 0, ret);
		return ret;
	} 

	/*test i2c interface and ensure the chip is write*/
	if (ret != ICN6211_REG_0) {
		pr_err("[icn6202] val = 0x%x i2c is not ok or chip not write\n", ret);
		return -1;
	}

	pr_err("[icn6202] icn6202_probe End..\n");
	icn6202_i2c_client = client;
	return 0;
}

static const struct of_device_id icn6202_mipi_lvds_of_match[] = {
	{ .compatible = "qcom,icn6202_mipi_lvds",},
	{},
};

static const struct i2c_device_id icn6202_id[] = {
	{"icn6202_mipi_lvds", 0},
	{},
};

static const struct dev_pm_ops icn6202_pm_ops =
{ 
	.suspend = icn6202_suspend,
	.resume = icn6202_resume, 
};

static struct i2c_driver icn6202_driver = {
	.driver = {
		.name = "qcom,icn6202_mipi_lvds",
		.owner    = THIS_MODULE,
		.of_match_table = icn6202_mipi_lvds_of_match,
		.pm = &icn6202_pm_ops,
	},
	.probe    = icn6202_probe,
	.remove   = icn6202_remove,
	.id_table = icn6202_id,
};

static int __init icn6202_init(void)
{
	printk(KERN_ERR "[icn6202]: %s",__func__);
	return i2c_add_driver(&icn6202_driver);
}

static void __exit icn6202_exit(void)
{
	printk(KERN_ERR "[icn6202]: %s", __func__);
	i2c_del_driver(&icn6202_driver);
}

module_init(icn6202_init);
module_exit(icn6202_exit);

MODULE_AUTHOR("shimin@meigsmart.com");
MODULE_DESCRIPTION("ICN6202 MIPI-LVDS driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("i2c:icn6202_mipi_lvds");
