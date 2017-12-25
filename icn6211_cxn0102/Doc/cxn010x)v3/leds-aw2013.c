/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/leds-aw2013.h>

#include <linux/fs.h>  
#include <asm/uaccess.h>  
#include <linux/device.h>
#include <linux/cdev.h>


/* register address */
#define AW_REG_RESET			0x00
#define AW_REG_GLOBAL_CONTROL		0x01
#define AW_REG_LED_STATUS		0x02
#define AW_REG_LED_ENABLE		0x30
#define AW_REG_LED_CONFIG_BASE		0x31
#define AW_REG_LED_BRIGHTNESS_BASE	0x34
#define AW_REG_TIMESET0_BASE		0x37
#define AW_REG_TIMESET1_BASE		0x38

/* register bits */
#define AW2013_CHIPID			0x33
#define AW_LED_MOUDLE_ENABLE_MASK	0x01
#define AW_LED_FADE_OFF_MASK		0x40
#define AW_LED_FADE_ON_MASK		0x20
#define AW_LED_BREATHE_MODE_MASK	0x10
#define AW_LED_RESET_MASK		0x55

#define AW_LED_RESET_DELAY		8
#define AW2013_VDD_MIN_UV		2600000
#define AW2013_VDD_MAX_UV		3300000
#define AW2013_VI2C_MIN_UV		1800000
#define AW2013_VI2C_MAX_UV		1800000

#define MAX_RISE_TIME_MS		7
#define MAX_HOLD_TIME_MS		5
#define MAX_FALL_TIME_MS		7
#define MAX_OFF_TIME_MS			5

//zouwubin NJDD 740 only one chip 2016.02.19
#define AW2013_SINGLE_CHIP
//NJDD 740 no need use regulator for vdd and vcc
#define INTERNAL_POWER

#define START_LED_BRITGHTNESS 40 //开机led全部点亮亮度值
#define START_LED_DELAY 4000 //开机LED全部点亮持续时间 ms

struct aw2013_led {
	struct i2c_client *client;
	struct led_classdev cdev;
	struct aw2013_platform_data *pdata;
	struct work_struct brightness_work;
	struct mutex lock;
//NJDD 740 no need use regulator for vdd and vcc 2016.02.19
#ifndef INTERNAL_POWER
	struct regulator *vdd;
	struct regulator *vcc;
#endif
	int num_leds;
	int id;
	bool poweron;
	
	struct workqueue_struct *workq;
	struct delayed_work aw2013_delay_work;
};

static int aw2013_write(struct aw2013_led *led, u8 reg, u8 val)
{
	return i2c_smbus_write_byte_data(led->client, reg, val);
}

static int aw2013_power_init(struct aw2013_led *led, bool on)
{
	int rc;

//NJDD 740 no need use regulator for vdd and vcc 2016.02.19
#ifdef INTERNAL_POWER
	rc = 0;
#else
	if (on) {
		led->vdd = regulator_get(&led->client->dev, "vdd");
		if (IS_ERR(led->vdd)) {
			rc = PTR_ERR(led->vdd);
			dev_err(&led->client->dev,
				"Regulator get failed vdd rc=%d\n", rc);
			return rc;
		}

		if (regulator_count_voltages(led->vdd) > 0) {
			rc = regulator_set_voltage(led->vdd, AW2013_VDD_MIN_UV,
						   AW2013_VDD_MAX_UV);
			if (rc) {
				dev_err(&led->client->dev,
					"Regulator set_vtg failed vdd rc=%d\n",
					rc);
				goto reg_vdd_put;
			}
		}

		led->vcc = regulator_get(&led->client->dev, "vcc");
		if (IS_ERR(led->vcc)) {
			rc = PTR_ERR(led->vcc);
			dev_err(&led->client->dev,
				"Regulator get failed vcc rc=%d\n", rc);
			goto reg_vdd_set_vtg;
		}

		if (regulator_count_voltages(led->vcc) > 0) {
			rc = regulator_set_voltage(led->vcc, AW2013_VI2C_MIN_UV,
						   AW2013_VI2C_MAX_UV);
			if (rc) {
				dev_err(&led->client->dev,
				"Regulator set_vtg failed vcc rc=%d\n", rc);
				goto reg_vcc_put;
			}
		}
	} else {
		if (regulator_count_voltages(led->vdd) > 0)
			regulator_set_voltage(led->vdd, 0, AW2013_VDD_MAX_UV);

		regulator_put(led->vdd);

		if (regulator_count_voltages(led->vcc) > 0)
			regulator_set_voltage(led->vcc, 0, AW2013_VI2C_MAX_UV);

		regulator_put(led->vcc);
	}
	return 0;

reg_vcc_put:
	regulator_put(led->vcc);
reg_vdd_set_vtg:
	if (regulator_count_voltages(led->vdd) > 0)
		regulator_set_voltage(led->vdd, 0, AW2013_VDD_MAX_UV);
reg_vdd_put:
	regulator_put(led->vdd);
#endif
	return rc;
}



/*****************************************************************/
/*************************cdev************************************/
/*****************************************************************/
#define ET6296_I2C_DEV

#ifdef ET6296_I2C_DEV

struct mutex et_lock;
struct aw2013_led *__led_array__;

#define DEVICE_SUM 1


void __et6269_menu_(void)
{
		aw2013_write(__led_array__,0x7f,0x00);//显示控制寄存器---关
		aw2013_write(__led_array__,0x00,0xb4);//模式控制寄存器
		aw2013_write(__led_array__,0x00,0x34);
		aw2013_write(__led_array__,0x01,0x00);//唤醒控制以及中断使能寄存器
		aw2013_write(__led_array__,0x02,0X00);//中断清除寄存器
		
		aw2013_write(__led_array__,0x03,0xff);//模式选择寄存器1
		aw2013_write(__led_array__,0x04,0xff);//模式选择寄存器2
		aw2013_write(__led_array__,0x05,0xff);//模式选择寄存器3
		aw2013_write(__led_array__,0x06,0xff);//模式选择寄存器4
		aw2013_write(__led_array__,0x07,0xff);//模式选择寄存器5
		aw2013_write(__led_array__,0x08,0xff);//模式选择寄存器6
		aw2013_write(__led_array__,0x09,0xff);//模式选择寄存器7

}
void __et6269_auto_(void)
{
	
		aw2013_write(__led_array__,0x7f,0x00);//显示控制寄存器---关
		//aw2013_write(__led_array__,0x00,0xb4);//模式控制寄存器
		aw2013_write(__led_array__,0x00,0x34);
		aw2013_write(__led_array__,0x01,0x80);//唤醒控制以及中断使能寄存器
		aw2013_write(__led_array__,0x02,0X00);//中断清除寄存器
		
		aw2013_write(__led_array__,0x03,0x00);//模式选择寄存器1
		aw2013_write(__led_array__,0x04,0x00);//模式选择寄存器2
		aw2013_write(__led_array__,0x05,0x00);//模式选择寄存器3
		aw2013_write(__led_array__,0x06,0x00);//模式选择寄存器4
		aw2013_write(__led_array__,0x07,0x00);//模式选择寄存器5
		aw2013_write(__led_array__,0x08,0x00);//模式选择寄存器6
		aw2013_write(__led_array__,0x09,0x00);//模式选择寄存器7

}

static int et6296_cdev_open(struct inode *inode, struct file *filp);
static int et6296_cdev_release(struct inode *, struct file *filp);
static long et6296_cdev_ioctl(struct file   *file,unsigned int  cmd,unsigned long data);

/* the major device number */
static int et6296_cdev_major = 0;
static int et6296_cdev_minor = 0;


static struct class* et6296_cdev_class = NULL;
/* define a cdev device */
static struct cdev *cdev;


/* init the file_operations structure */
struct file_operations et6296_cdev_fops =
{
    .owner = THIS_MODULE,
    .open = et6296_cdev_open,
    .release = et6296_cdev_release,
    .unlocked_ioctl = et6296_cdev_ioctl,
    //.read = et6296_cdev_read,
    //.write = et6296_cdev_write,
};

/* open device */
static int et6296_cdev_open(struct inode *inode, struct file *filp)
{
    int ret = 0;
    printk(KERN_ERR "eliot:et6296_cdev_open success.\n");
    return ret;
}

#define ET6296_POWER_DOWN	0x22
#define ET6296_ALL_OFF		0x33
#define ET6296_ALL_OPEN		0x44
#define ET6296_ALL_MANU		0x55
#define ET6296_ALL_AUTO		0x66
#define ET6296_USER_MODE	0x77
#define ET6296_ALL_IN_ONE    0x88


#define _SDRAM_SIZE 1788


void et6292_write_ram_single(unsigned int ram_addr,unsigned char dataH,unsigned char dataL)
{
	unsigned char datax[3]={0};
	unsigned int temp ;
	unsigned char commandx;
	temp = ram_addr ;
	datax[0] = (unsigned char)ram_addr ;
	commandx = (temp>>8) | 0x80;
	datax[1] = dataH ;
	datax[2] = dataL ;
	i2c_smbus_write_i2c_block_data(__led_array__->client,commandx,3,datax);
}
static int et6292_ioctl(struct file   *file,
		      unsigned int  cmd,
		      unsigned long data)
{
	int ret ;
	int i=0;
	unsigned char led_effect[1800]={0};

	/*
		led_effect[0]--led_effect[6] -->reg03-reg09 for menumode or auto mode
		led_effect[7]--led_effect[60] 54bytes -->reg0a-reg3f for automode 
		led_effect[61]--led_effect[1788]  1730bytes ram for automode
		
	*/
	void __user *arg = (void __user *)data;
	switch ( cmd )
	{
		case ET6296_POWER_DOWN:
			printk(KERN_ERR "eliot:et6292_ioctl ET6296_POWER_DOWN.\n");
			aw2013_write(__led_array__,0x02,0X00);//中断清除寄存器
			aw2013_write(__led_array__,0x00,0xB4);
			break;
		case ET6296_ALL_OFF:
			printk(KERN_ERR "eliot:et6292_ioctl ET6296_ALL_OFF.\n");
			aw2013_write(__led_array__,0x02,0X00);//中断清除寄存器
			aw2013_write(__led_array__,0x7f,0x0);
			break;
		case ET6296_ALL_OPEN:
			printk(KERN_ERR "eliot:et6292_ioctl ET6296_ALL_OPEN.\n");
			aw2013_write(__led_array__,0x7f,0x80);
			break;
		case ET6296_ALL_MANU:
			printk(KERN_ERR "eliot:et6292_ioctl ET6296_MENU.\n");
			__et6269_menu_();
			//aw2013_write(__led_array__,0x7f,0x80);
			break;
		case ET6296_ALL_AUTO:
			printk(KERN_ERR "eliot:et6292_ioctl ET6296_AUTO.\n");
			if (copy_from_user(led_effect, arg, _SDRAM_SIZE)) {
				printk(KERN_ERR "eliot:et6292_ioctl copy_from_user err.\n");
				ret= -EFAULT;
				break;
			}
			__et6269_auto_();

			//clear ram
			for(i=0;i<864;i++)
			{
				et6292_write_ram_single(i,0,0);
			}

			for(i=0;i<864;i++)
			{
				et6292_write_ram_single(i,led_effect[61+i*2],led_effect[62+i*2]);
			}
			
			//et6292_write_ram_single(0,0x20,0x9F);
			//et6292_write_ram_single(1,0x3F,0x80);

			for(i=0;i<54;i++)
			{	 	
			 	aw2013_write(__led_array__,0x0a+i,led_effect[i+7]);
			}
			//aw2013_write(__led_array__,0x7f,0x80);
			break;
		case ET6296_USER_MODE:
			printk(KERN_ERR "eliot:et6292_ioctl ET6296_USER.\n");
			if (copy_from_user(led_effect, arg, _SDRAM_SIZE)) {
				printk(KERN_ERR "eliot:et6292_ioctl copy_from_user err.\n");
				ret= -EFAULT;
				break;
			}
			aw2013_write(__led_array__,0x7f,0x00);//显示控制寄存器---关
			aw2013_write(__led_array__,0x00,0x34);
			aw2013_write(__led_array__,0x01,0x80);//唤醒控制以及中断使能寄存器
			aw2013_write(__led_array__,0x02,0X00);//中断清除寄存器
			for(i=0;i<7;i++)
			{
				aw2013_write(__led_array__,0x03+i,led_effect[i]);
			}
			//clear ram
			for(i=0;i<864;i++)
			{
				et6292_write_ram_single(i,0,0);
			}

			for(i=0;i<864;i++)
			{
				et6292_write_ram_single(i,led_effect[61+i*2],led_effect[62+i*2]);
			}
			for(i=0;i<54;i++)
			{	 	
			 	aw2013_write(__led_array__,0x0a+i,led_effect[i+7]);
			}
			
			//aw2013_write(__led_array__,0x7f,0x80);
			break;


		case ET6296_ALL_IN_ONE:
			{
				#define LIGHT_NUM 54
				unsigned char brits[LIGHT_NUM+4]={0};
				if (copy_from_user(brits, arg, LIGHT_NUM)) {
					printk(KERN_ERR "eliot:et6292_ioctl copy_from_user err.\n");
					ret= -EFAULT;
					break;
				}

				aw2013_write(__led_array__,0x7f,0x80);
				for( i = 0; i < LIGHT_NUM; i++ )
				{
					aw2013_write(__led_array__, (0x49+i), brits[i]);
				}
				ret = 0;
			}
			break;

		default:
			ret = -EINVAL;
	}
	return ret ;
}

static long et6296_cdev_ioctl(struct file   *file,
			        unsigned int  cmd,
			        unsigned long data)
{
	int ret;
	printk(KERN_ERR"eliot:et6296_cdev_ioctl ..\n");

	mutex_lock(&et_lock);
	ret = et6292_ioctl(file, cmd, data);
	mutex_unlock(&et_lock);

	return ret;
}

/* release device */
static int et6296_cdev_release(struct inode *inode, struct file *filp)
{
    printk(KERN_ERR"eliot:et6296_cdev_release release success.\n");
    return 0;
}

#endif
/*****************************************************************/
/*************************cdev************************************/
/*****************************************************************/

void write_cmd_more(struct aw2013_led *led,unsigned char addr,unsigned char n,unsigned char w_data[])
{

	 int i;
	 for(i=0;i<n;i++)
	 {	 	
	 	aw2013_write(led,addr+i,w_data[i]);//命令模式对应的数据
	 }

}
void init_ET6296(struct aw2013_led *led_array)
{
	printk(KERN_ERR"init_ET6296--------\n");
	aw2013_write(led_array,0x7f,0x00);//显示控制寄存器---关
	aw2013_write(led_array,0x00,0xb4);//模式控制寄存器
	aw2013_write(led_array,0x00,0x34);
	aw2013_write(led_array,0x01,0x00);//唤醒控制以及中断使能寄存器
	aw2013_write(led_array,0x02,0X00);//中断清除寄存器
	//aw2013_write(led_array,0x7f,0x80);//显示控制寄存器---开
}

void selet_ET6296_model(struct aw2013_led *led_array,const char  dat)
{
	printk(KERN_ERR"selet_ET6296_model--------\n");

	aw2013_write(led_array,0x03,dat);//模式选择寄存器1
	aw2013_write(led_array,0x04,dat);//模式选择寄存器2
	aw2013_write(led_array,0x05,dat);//模式选择寄存器3
	aw2013_write(led_array,0x06,dat);//模式选择寄存器4
	aw2013_write(led_array,0x07,dat);//模式选择寄存器5
	aw2013_write(led_array,0x08,dat);//模式选择寄存器6
	aw2013_write(led_array,0x09,dat);//模式选择寄存器7


}


void ET6296_test(struct aw2013_led *led_array)//全部点亮测试
{
 	 char i;
	 selet_ET6296_model(led_array,0xff);

	for(i=0;i<54;i++)//54个led逐个赋值
	{
	   aw2013_write(led_array, (0x49+i), START_LED_BRITGHTNESS);
	   mdelay(1);
	}
}

void set_channel_light(struct aw2013_led *led,unsigned char LEDn,unsigned char value)//LEDn-->led0-53,value-->亮度0-127
{
	printk(KERN_ERR"---aw2013_brightness_work----value=%d,LEDn=%d\n",LEDn,value);
	aw2013_write(led,0x7f,0x80);//显示控制寄存器---开
	//固定亮度寄存器
	aw2013_write(led,(0x49+LEDn),value);
}
void set_all_light_swith(struct aw2013_led *led,int onoff)
{
	 char i;
	 
	printk(KERN_ERR"---set_all_light_swith----onoff=%d\n",onoff);
	aw2013_write(led,0x7f,0x80);//显示控制寄存器---开
	if(onoff == 1)
	{
		for(i=0;i<54;i++)//54个led逐个赋值
		{
		   aw2013_write(led, (0x49+i), 0x7f);
		   mdelay(1);
		}
		//set_channel_light(led, 0x00,0x7f);//white light on
		//set_channel_light(led, 0x07,0x7f);//white light on
		//set_channel_light(led, 0x0e,0x7f); //red light on
		//set_channel_light(led, 0x15,0x7f); //red  light on	
	}
	else
	{
		for(i=0;i<54;i++)//54个led逐个赋值
		{
		   aw2013_write(led, (0x49+i), 0);
		   mdelay(1);
		}
		//set_channel_light(led, 0x00,0x00);//white light off
		//set_channel_light(led, 0x07,0x00);//white light off
		//set_channel_light(led, 0x0e,0x00); //red light off
		//set_channel_light(led, 0x15,0x00); //red  light off	
	}		
}



static void aw2013_brightness_work(struct work_struct *work)
{
	struct aw2013_led *led = container_of(work, struct aw2013_led,
					brightness_work);
	mutex_lock(&led->pdata->led->lock);

	if (led->cdev.brightness > 0) {
		set_channel_light(led, led->id, led->cdev.brightness);
	} else {
		set_channel_light( led, led->id, led->cdev.brightness);
	}



	mutex_unlock(&led->pdata->led->lock);
}

	
static void aw2013_led_blink_set(struct aw2013_led *led, unsigned long blinking)
{
#if 1
	unsigned char  RGB_brigh1[]=
	{
		 0,0,0,0,100,65,0,0,0,
		64,80,45,0,0,0,127,25,30,
		0,0,0,90,127,70,0,0,0,
		112,0,100,0,0,0,32,127,0,
		0,0,0,120,21,127,0,0,0,
		16,21,127,0,0,0,80,64,120
	};
	printk(KERN_ERR"---aw2013_led_blink_set----blinking=%ld,led->id=%d\n",blinking,led->id);
	if(blinking == 2) {
		 //单点全屏
		  aw2013_write(led,0x7f,0x00);//关显示
		  aw2013_write(led,0x49+led->id,0x7F);
		 aw2013_write(led,0x7f,0x80);//开显示
	}else if(blinking == 3) {
		 //单点全屏
		  aw2013_write(led,0x7f,0x00);//关显示
		  write_cmd_more(led,0x49,54,RGB_brigh1);
		 aw2013_write(led,0x7f,0x80);//开显示
	}else if (blinking == 4){
		ET6296_test(led);
	}else if (blinking == 5){
		set_all_light_swith(led, 0);
	}else if (blinking == 6){
		set_all_light_swith(led, 1);
	}
	#endif	



	
}
static void aw2013_set_brightness(struct led_classdev *cdev,
			     enum led_brightness brightness)
{
	struct aw2013_led *led = container_of(cdev, struct aw2013_led, cdev);
	led->cdev.brightness = brightness;

	schedule_work(&led->brightness_work);
}
static ssize_t aw2013_store_ledbrightness(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t len)
{
	unsigned long blinking;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw2013_led *led =
			container_of(led_cdev, struct aw2013_led, cdev);
	ssize_t ret = -EINVAL;

	ret = kstrtoul(buf, 10, &blinking);
	if (ret)
		return ret;
	mutex_lock(&led->pdata->led->lock);
	aw2013_led_blink_set(led, blinking);
	mutex_unlock(&led->pdata->led->lock);

	return len;
}

static ssize_t aw2013_store_blink(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t len)
{
	unsigned long blinking;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw2013_led *led =
			container_of(led_cdev, struct aw2013_led, cdev);
	ssize_t ret = -EINVAL;

	ret = kstrtoul(buf, 10, &blinking);
	if (ret)
		return ret;
	mutex_lock(&led->pdata->led->lock);
	aw2013_led_blink_set(led, blinking);
	mutex_unlock(&led->pdata->led->lock);

	return len;
}

static ssize_t aw2013_led_time_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw2013_led *led =
			container_of(led_cdev, struct aw2013_led, cdev);

	return snprintf(buf, PAGE_SIZE, "%d %d %d %d\n",
			led->pdata->rise_time_ms, led->pdata->hold_time_ms,
			led->pdata->fall_time_ms, led->pdata->off_time_ms);
}

static ssize_t aw2013_led_time_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t len)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct aw2013_led *led =
			container_of(led_cdev, struct aw2013_led, cdev);
	int rc, rise_time_ms, hold_time_ms, fall_time_ms, off_time_ms;

	rc = sscanf(buf, "%d %d %d %d",
			&rise_time_ms, &hold_time_ms,
			&fall_time_ms, &off_time_ms);

	mutex_lock(&led->pdata->led->lock);
	led->pdata->rise_time_ms = (rise_time_ms > MAX_RISE_TIME_MS) ?
				MAX_RISE_TIME_MS : rise_time_ms;
	led->pdata->hold_time_ms = (hold_time_ms > MAX_HOLD_TIME_MS) ?
				MAX_HOLD_TIME_MS : hold_time_ms;
	led->pdata->fall_time_ms = (fall_time_ms > MAX_FALL_TIME_MS) ?
				MAX_FALL_TIME_MS : fall_time_ms;
	led->pdata->off_time_ms = (off_time_ms > MAX_OFF_TIME_MS) ?
				MAX_OFF_TIME_MS : off_time_ms;
	aw2013_led_blink_set(led, 1);
	mutex_unlock(&led->pdata->led->lock);
	return len;
}

static DEVICE_ATTR(blink, 0664, NULL, aw2013_store_blink);
static DEVICE_ATTR(ledbrightness, 0664, NULL, aw2013_store_ledbrightness);
static DEVICE_ATTR(led_time, 0664, aw2013_led_time_show, aw2013_led_time_store);

static struct attribute *aw2013_led_attributes[] = {
	&dev_attr_blink.attr,
	&dev_attr_led_time.attr,
	&dev_attr_ledbrightness.attr,
	NULL,
};

static struct attribute_group aw2013_led_attr_group = {
	.attrs = aw2013_led_attributes
};



static int aw2013_led_err_handle(struct aw2013_led *led_array,
				int parsed_leds)
{
	int i;
	/*
	 * If probe fails, cannot free resource of all LEDs, only free
	 * resources of LEDs which have allocated these resource really.
	 */
	for (i = 0; i < parsed_leds; i++) {
		sysfs_remove_group(&led_array[i].cdev.dev->kobj,
				&aw2013_led_attr_group);
		led_classdev_unregister(&led_array[i].cdev);
		cancel_work_sync(&led_array[i].brightness_work);
		devm_kfree(&led_array->client->dev, led_array[i].pdata);
		led_array[i].pdata = NULL;
	}
	return i;
}

static int aw2013_led_parse_child_node(struct aw2013_led *led_array,
				struct device_node *node)
{

	struct aw2013_led *led;
	//struct device_node *temp;
	struct aw2013_platform_data *pdata;
	int rc = 0, parsed_leds = 0;
	int i,j=0;
	
	//for_each_child_of_node(node, temp) 
        for(i=0;i<54; i++)
	{
		 char *buf ="";
		//printk(KERN_ERR"----aw2013_led_parse_child_node\n");
		led = &led_array[parsed_leds];
		led->client = led_array->client;

		pdata = devm_kzalloc(&led->client->dev,
				sizeof(struct aw2013_platform_data),
				GFP_KERNEL);
		if (!pdata) {
			dev_err(&led->client->dev,
				"Failed to allocate memory\n");
			goto free_err;
		}
		pdata->led = led_array;
		led->pdata = pdata;
		 snprintf(buf, 6, "%s%d", "red",j);
		 		//printk(KERN_ERR"----aw2013_led_parse_child_node,buf=%s\n",buf);

		//led->cdev.name = "red"
		led->cdev.name = buf;
		//printk(KERN_ERR"----aw2013_led_parse_child_node,led->cdev.name=%s\n",led->cdev.name);

		led->id  = j;	
		led->cdev.max_brightness = 127;
		led->pdata->max_current =40;
		led->pdata->rise_time_ms =10;
		led->pdata->rise_time_ms = 10;
		led->pdata->rise_time_ms = 10;
		led->pdata->fall_time_ms = 10;
		led->pdata->hold_time_ms = 10;
		led->pdata->off_time_ms = 10;
		#if 0
		rc = of_property_read_string(temp, "aw2013,name",
			&led->cdev.name);
		if (rc < 0) {
			dev_err(&led->client->dev,
				"Failure reading led name, rc = %d\n", rc);
			goto free_pdata;
		}
		
		rc = of_property_read_u32(temp, "aw2013,id",
			&led->id);
		if (rc < 0) {
			dev_err(&led->client->dev,
				"Failure reading id, rc = %d\n", rc);
			goto free_pdata;
		}

		rc = of_property_read_u32(temp, "aw2013,max-brightness",
			&led->cdev.max_brightness);
		if (rc < 0) {
			dev_err(&led->client->dev,
				"Failure reading max-brightness, rc = %d\n",
				rc);
			goto free_pdata;
		}

		rc = of_property_read_u32(temp, "aw2013,max-current",
			&led->pdata->max_current);
		if (rc < 0) {
			dev_err(&led->client->dev,
				"Failure reading max-current, rc = %d\n", rc);
			goto free_pdata;
		}

		rc = of_property_read_u32(temp, "aw2013,rise-time-ms",
			&led->pdata->rise_time_ms);
		if (rc < 0) {
			dev_err(&led->client->dev,
				"Failure reading rise-time-ms, rc = %d\n", rc);
			goto free_pdata;
		}

		rc = of_property_read_u32(temp, "aw2013,hold-time-ms",
			&led->pdata->hold_time_ms);
		if (rc < 0) {
			dev_err(&led->client->dev,
				"Failure reading hold-time-ms, rc = %d\n", rc);
			goto free_pdata;
		}

		rc = of_property_read_u32(temp, "aw2013,fall-time-ms",
			&led->pdata->fall_time_ms);
		if (rc < 0) {
			dev_err(&led->client->dev,
				"Failure reading fall-time-ms, rc = %d\n", rc);
			goto free_pdata;
		}

		rc = of_property_read_u32(temp, "aw2013,off-time-ms",
			&led->pdata->off_time_ms);
		if (rc < 0) {
			dev_err(&led->client->dev,
				"Failure reading off-time-ms, rc = %d\n", rc);
			goto free_pdata;
		}
		#endif
		INIT_WORK(&led->brightness_work, aw2013_brightness_work);

		led->cdev.brightness_set = aw2013_set_brightness;

		rc = led_classdev_register(&led->client->dev, &led->cdev);
		if (rc) {
			dev_err(&led->client->dev,
				"unable to register led %d,rc=%d\n",
				led->id, rc);
			goto free_pdata;
		}

		rc = sysfs_create_group(&led->cdev.dev->kobj,
				&aw2013_led_attr_group);
		if (rc) {
			dev_err(&led->client->dev, "led sysfs rc: %d\n", rc);
			goto free_class;
		}
		parsed_leds++;
		j++;
	}

	return 0;

free_class:
	aw2013_led_err_handle(led_array, parsed_leds);
	led_classdev_unregister(&led_array[parsed_leds].cdev);
	cancel_work_sync(&led_array[parsed_leds].brightness_work);
	devm_kfree(&led->client->dev, led_array[parsed_leds].pdata);
	led_array[parsed_leds].pdata = NULL;
	return rc;

free_pdata:
	aw2013_led_err_handle(led_array, parsed_leds);
	devm_kfree(&led->client->dev, led_array[parsed_leds].pdata);
	return rc;

free_err:
	aw2013_led_err_handle(led_array, parsed_leds);

	return rc;
}

#if 0

static int aw2013_register_chg_led(struct aw2013_led *chip)
{
	int rc;

	chip->cdev.name = "red";
	//chip->cdev.brightness_set = smbchg_chg_led_brightness_set;
	//chip->cdev.brightness_get = smbchg_chg_led_brightness_get;
	//chip->cdev.brightness_set = aw2013_set_brightness;
	rc = led_classdev_register(&chip->client->dev, &chip->cdev);
	if (rc) {
		//dev_err(chip->dev, "unable to register charger led, rc=%d\n",
		//		rc);
		return rc;
	}

	rc = sysfs_create_group(&chip->cdev.dev->kobj,
			&aw2013_led_attr_group);
	if (rc) {
		//dev_err(chip->dev, "led sysfs rc: %d\n", rc);
		return rc;
	}

	return rc;
}
#endif

/*
void write_data_bolang(struct aw2013_led *led)
{
	unsigned char ram_data[1800]={0};
	int i=0,j=0,k=2;
		unsigned char tempL,tempH;
	ram_data[0]=0x80;
	ram_data[1]=0x00;

	for(j=0;j<54;j++)
	{
		for(i=0;i<16;i++)
		{
			
			tempL =(unsigned char)ram[j][i];
			tempH =(unsigned char)(ram[j][i]>>8);
			ram_data[k] = tempH ;
			ram_data[k++] = tempL ;
			k++;
		}
	}
	printk(KERN_ERR "kkkkkkkk=%d\n",k);
	//i2c_smbus_write_i2c_block_data(led->client,0x80,127,ram_data);
	i2c_master_send(led->client,ram_data,1730);
	
}
*/
static void aw2013_delay_work_shutdown(struct work_struct *work)
{
 	 char i;
	 printk(KERN_ERR "----aw2013_delay_work_shutdown----\n");
	 selet_ET6296_model(__led_array__,0xff);
	aw2013_write(__led_array__,0x7f,0x80);
	for(i=0;i<54;i++)//54个led逐个赋值
	{
	   aw2013_write(__led_array__, (0x49+i), 0);
	   mdelay(1);
	}

}
static int aw2013_led_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct aw2013_led *led_array;
	struct device_node *node;
	int ret, num_leds = 54;
	//u8 val;
#ifdef ET6296_I2C_DEV
		//int err = 0 ;
		struct device* temp = NULL;
		dev_t devno = 0;
#endif
	node = client->dev.of_node;
	if (node == NULL)
		return -EINVAL;

	num_leds = 54;//of_get_child_count(node);

	if (!num_leds)
		return -EINVAL;
	printk(KERN_ERR "----aw2013_led_probe----\n");
	led_array = devm_kzalloc(&client->dev,
			(sizeof(struct aw2013_led) * num_leds), GFP_KERNEL);
	if (!led_array) {
		dev_err(&client->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}
	led_array->client = client;
	led_array->num_leds = num_leds;
	mutex_init(&led_array->lock);
	/*add by eliot shao 2017.2.9*/
#ifdef ET6296_I2C_DEV
	__led_array__ = led_array ;

	mutex_init(&et_lock);

	ret= alloc_chrdev_region(&devno, et6296_cdev_minor, DEVICE_SUM, "et6296led");
    if(ret < 0) {
        printk(KERN_ALERT "et6296_cdev:Failed to alloc char dev region.\n");
        goto free_led_arry;
    }
	
	et6296_cdev_major = MAJOR(devno);
	et6296_cdev_minor = MINOR(devno);
	cdev = cdev_alloc();
    cdev->owner = THIS_MODULE;
    cdev->ops = &et6296_cdev_fops;
    if ((ret = cdev_add(cdev, devno, 1)))
    {
        printk(KERN_NOTICE "et6296_cdev:Error %d adding et6296_cdev.\n", ret);
        goto free_cdev;
    }
    else
        printk(KERN_ALERT "et6296_cdev:cxn_cdev register success.\n");
        /*在/sys/class/目录下创建设备类别目录*/
    et6296_cdev_class = class_create(THIS_MODULE, "et6296led");
    if(IS_ERR(et6296_cdev_class)) {
        ret = PTR_ERR(et6296_cdev_class);
        printk(KERN_ALERT"Failed to create et6296_cdev class.\n");
        goto free_cdev;
    }        
    /*在/dev/目录和/sys/class/cxn_cdev目录下分别创建设备文件*/
    temp = device_create(et6296_cdev_class, NULL, devno, "%s", "et6296led");
    if(IS_ERR(temp)) {
        ret = PTR_ERR(temp);
        printk(KERN_ALERT"Failed to create cxn_cdev device.");
        goto destroy_class;
    }    
#endif

	/*ret = aw_2013_check_chipid(led_array);
	if (ret) {
		dev_err(&client->dev, "Check chip id error\n");
		goto free_led_arry;
	}*/

	ret = aw2013_led_parse_child_node(led_array, node);
	if (ret) {
		dev_err(&client->dev, "parsed node error\n");
		goto free_led_arry;
	}

	i2c_set_clientdata(client, led_array);

	ret = aw2013_power_init(led_array, true);
	if (ret) {
		dev_err(&client->dev, "power init failed");
		goto fail_parsed_node;
	}


		led_array->workq = create_workqueue("aw2013_workq");
		if (!led_array->workq) {
			pr_err("%s: workqueue creation failed.\n", __func__);
			ret = -EPERM;
			goto fail_parsed_node;
		}

		INIT_DELAYED_WORK(&led_array->aw2013_delay_work, aw2013_delay_work_shutdown);

		queue_delayed_work(led_array->workq, &led_array->aw2013_delay_work, msecs_to_jiffies(START_LED_DELAY));

	//aw2013_register_chg_led(led_array);


	init_ET6296(led_array);
	//set_all_light_swith(led_array, 0);
	selet_ET6296_model(led_array, 0xFF);
	//write_cmd_more(led_array,0x0a,54,reg_bolang);
	//write_data_bolang(led_array);
	aw2013_write(led_array,0x7f,0x80);
	 ET6296_test(led_array);//全部点亮测试


	//aw2013_read(led_array, 0x49, &val);

	//printk(KERN_ERR"--aw2013_led_probe----val=%d\n",val);


	
	printk(KERN_ERR"aw2013_led_probe--------success\n");
	return 0;

fail_parsed_node:
	aw2013_led_err_handle(led_array, num_leds);
	
#ifdef ET6296_I2C_DEV
destroy_class:
	class_destroy(et6296_cdev_class);
free_cdev:
		cdev_del(cdev);
#endif

free_led_arry:
	mutex_destroy(&led_array->lock);
#ifdef ET6296_I2C_DEV
	mutex_destroy(&et_lock);
#endif
	devm_kfree(&client->dev, led_array);
	led_array = NULL;
	return ret;
}

static int aw2013_led_remove(struct i2c_client *client)
{
	struct aw2013_led *led_array = i2c_get_clientdata(client);
	int i, parsed_leds = led_array->num_leds;

	for (i = 0; i < parsed_leds; i++) {
		sysfs_remove_group(&led_array[i].cdev.dev->kobj,
				&aw2013_led_attr_group);
		led_classdev_unregister(&led_array[i].cdev);
		cancel_work_sync(&led_array[i].brightness_work);
		devm_kfree(&client->dev, led_array[i].pdata);
		led_array[i].pdata = NULL;
	}
	mutex_destroy(&led_array->lock);
	devm_kfree(&client->dev, led_array);
	led_array = NULL;
	return 0;
}

static const struct i2c_device_id aw2013_led_id[] = {
	{"aw2013_led", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, aw2013_led_id);

static struct of_device_id aw2013_match_table[] = {
	{ .compatible = "awinic,aw2013",},
	{ },
};

static struct i2c_driver aw2013_led_driver = {
	.probe = aw2013_led_probe,
	.remove = aw2013_led_remove,
	.driver = {
		.name = "aw2013_led",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(aw2013_match_table),
	},
	.id_table = aw2013_led_id,
};

static int __init aw2013_led_init(void)
{
	printk(KERN_ERR"----aw2013_led_init-----\n");
	return i2c_add_driver(&aw2013_led_driver);
}
module_init(aw2013_led_init);

static void __exit aw2013_led_exit(void)
{
	i2c_del_driver(&aw2013_led_driver);
}
module_exit(aw2013_led_exit);

MODULE_DESCRIPTION("AWINIC aw2013 LED driver");
MODULE_LICENSE("GPL v2");
