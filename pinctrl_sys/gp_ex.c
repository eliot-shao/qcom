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

unsigned long isGPIO =0;  
struct pinctrl *gp_pinctrl;
struct pinctrl_state *gp_default;//uart
struct pinctrl_state *gp_gpio;//gpio

const char* gp_PINCTRL_STATE_DEFAULT = "default";
const char* gp_PINCTRL_STATE_GPIO = "gpio";

static int gp_request_gpios(int state)
{
	int result = 0;

	if(state){
		result = pinctrl_select_state(gp_pinctrl, gp_default);
		if (result) {
			printk("%s: Can not set %s pins\n",
			__func__, "default");
		}
	}else{
		result = pinctrl_select_state(gp_pinctrl, gp_gpio);
		if (result) {
			printk("%s: Can not set %s pins\n",
			__func__, "gpio");
		}
	}
	printk(KERN_ERR "%s, eliot request state(%s) ok, result = %d\n",
			__func__, state ? "default" : "gpio", result);

		return result;
}
static ssize_t lock_show(struct device* cd,struct device_attribute *attr, char* buf)  
{  
	ssize_t ret = 0;  
	sprintf(buf, "%lu\n",isGPIO); 	
	ret = strlen(buf) + 1;
	return ret;  
}  

static ssize_t lock_store(struct device* cd, struct device_attribute *attr,  
const char* buf, size_t len)  
{  
	unsigned long on_off = simple_strtoul(buf, NULL, 10);  
	isGPIO = on_off;  
	printk("%s: %lu\n",__func__, isGPIO);  
	gp_request_gpios(!!isGPIO);

	if(!isGPIO){
		printk(KERN_ERR "set gpio 4 & 5to 0\n");
		gpio_set_value(4 + 911, !!isGPIO);
		gpio_set_value(5 + 911, !!isGPIO);
	}
	return len;  
}
  
static DEVICE_ATTR(gp_ex, S_IRUGO | S_IWUSR, lock_show, lock_store);  


static int gp_pinctrl_init(struct platform_device *pdata)
{
	gp_pinctrl = devm_pinctrl_get(&pdata->dev);
	if (IS_ERR_OR_NULL(gp_pinctrl)) {
		printk("Failed to get pin ctrl\n");
		return PTR_ERR(gp_pinctrl);
	}

	// default
	gp_default = pinctrl_lookup_state(gp_pinctrl,
				gp_PINCTRL_STATE_DEFAULT);
	if (IS_ERR_OR_NULL(gp_default)) {
		printk("Failed to lookup pinctrl default state\n");
		return PTR_ERR(gp_default);
	}

	// gpio
	gp_gpio = pinctrl_lookup_state(gp_pinctrl,
				gp_PINCTRL_STATE_GPIO);
	if (IS_ERR_OR_NULL(gp_gpio)) {
		printk("Failed to lookup pinctrl gpio state\n");
		return PTR_ERR(gp_gpio);
	}
	
	printk("eliot %s,---ok\n", __func__);
	return 0;

}
// echo 0 > /sys/bus/platform/devices/gpio_uart_exchage.67/gp_ex //设置为普通GPIO
// echo 1 > /sys/bus/platform/devices/gpio_uart_exchage.67/gp_ex //设置为UART模式
static int gp_ex_probe(struct platform_device *pdev)
{
	int ret = 0;
	printk(KERN_ERR "eliot :gp_ex_probe\n");
	gp_pinctrl_init(pdev);
if (device_create_file(&pdev->dev, &dev_attr_gp_ex))  
    printk(KERN_ERR "Unable to create sysfs entry: '%s'\n",  
            dev_attr_gp_ex.attr.name);  

	return ret;

}

static int gp_ex_remove(struct platform_device *plat)
{

	return 0;
}

static struct of_device_id __attribute__ ((unused)) gp_ex_of_match[] = {
	{ .compatible = "gpio-uart-exchage", },
	{}
};

static struct platform_driver gp_ex_driver = {
	.probe = gp_ex_probe,
	.remove = gp_ex_remove,
	.driver = {
		.name = "gpio-exchage",
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
MODULE_DESCRIPTION("Driver to exchager gpio4/5.");
MODULE_VERSION("1.0");

module_init(gp_ex_init);
module_exit(gp_ex_exit);
