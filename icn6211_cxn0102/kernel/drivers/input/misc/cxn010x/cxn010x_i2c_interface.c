/*
*author :shaomingliang (Eliot shao)
*version:1.0
*data:2016.12.16
*function description : an i2c interface for cxn0102
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
#include <linux/regulator/consumer.h>
#include <linux/notifier.h>
#include <linux/fb.h>


#include "cxn010x_i2c_interface.h"

struct cxn_i2c_interface {
	unsigned short flags;
	struct i2c_client *cxn_i2c_client;
	unsigned int slave_addr ;
	struct mutex lock;
	struct mutex i2c_lock;
	int gpio_rstn ;//for gpio25
	int gpio_irq ;
	struct pinctrl		*pinctrl;
	struct pinctrl_state	*pin_default;
	struct pinctrl_state	*pin_sleep;
	spinlock_t irq_lock;
	struct regulator *vdd;

	unsigned char notify_num ;
	struct workqueue_struct *cxn_wq;
	struct work_struct	work;

	unsigned char isActive;
	struct notifier_block fb_notif;

	//unsigned long isPower;
};

static struct cxn_i2c_interface *my_cxn_i2c ;

static int icn_reset_chip(void)
{

	gpio_direction_output(my_cxn_i2c->gpio_rstn, 0);
	
	mdelay(50);

	gpio_direction_output(my_cxn_i2c->gpio_rstn, 1);

	return 0;
}
#define PINCTRL_STATE_ACTIVE__	 	"pmx_ts_active"
#define PINCTRL_STATE_SUSPEND__	 	"pmx_ts_suspend"

static int cxn_pinctrl_init(struct cxn_i2c_interface *lt)
{
	int retval;
	struct i2c_client *client = lt->cxn_i2c_client;
	/* Get pinctrl if target uses pinctrl */
	lt->pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR_OR_NULL(lt->pinctrl)) {
		retval = PTR_ERR(lt->pinctrl);
		dev_dbg(&client->dev,
			"Target does not use pinctrl %d\n", retval);
		goto err_pinctrl_get;
	}
	lt->pin_default
		= pinctrl_lookup_state(lt->pinctrl,
				PINCTRL_STATE_ACTIVE__);
	if (IS_ERR_OR_NULL(lt->pin_default)) {
		retval = PTR_ERR(lt->pin_default);
		dev_err(&client->dev,
			"Can not lookup %s pinstate %d\n",
			PINCTRL_STATE_ACTIVE__, retval);
		goto err_pinctrl_lookup;
	}
	lt->pin_sleep
		= pinctrl_lookup_state(lt->pinctrl,
			PINCTRL_STATE_SUSPEND__);
	if (IS_ERR_OR_NULL(lt->pin_sleep)) {
		retval = PTR_ERR(lt->pin_sleep);
		dev_err(&client->dev,
			"Can not lookup %s pinstate %d\n",
			PINCTRL_STATE_SUSPEND__, retval);
		goto err_pinctrl_lookup;
	}

	return 0;

err_pinctrl_lookup:
	devm_pinctrl_put(lt->pinctrl);
err_pinctrl_get:
	lt->pinctrl = NULL;
	return retval;
}

void cxn_irq_disable(struct cxn_i2c_interface *cxn)
{
	unsigned long irqflags;

	spin_lock_irqsave(&cxn->irq_lock, irqflags);
	disable_irq_nosync(cxn->cxn_i2c_client->irq);
	spin_unlock_irqrestore(&cxn->irq_lock, irqflags);
}

void cxn_irq_enable(struct cxn_i2c_interface *cxn)
{
	unsigned long irqflags = 0;

	spin_lock_irqsave(&cxn->irq_lock, irqflags);
	enable_irq(cxn->cxn_i2c_client->irq);
	spin_unlock_irqrestore(&cxn->irq_lock, irqflags);
}

static irqreturn_t cxn_irq_handler(int irq, void *dev_id)
{
	struct cxn_i2c_interface *cxn = dev_id;
	

	cxn_irq_disable(cxn);
	//printk(KERN_ERR "eliot :cxn_irq_handler...");
	cxn->notify_num ++ ;
	queue_work(cxn->cxn_wq, &cxn->work);

	return IRQ_HANDLED;
}

static int cxn_request_irq(struct cxn_i2c_interface *ts)
{
	int ret = -1;

	ret = request_irq(ts->cxn_i2c_client->irq, cxn_irq_handler,
			  IRQ_TYPE_EDGE_RISING,
			  ts->cxn_i2c_client->name, ts);
	if (ret) {
		printk(KERN_ERR "Request IRQ failed!ERRNO:%d.", ret);
		return ret;
	} else {
		cxn_irq_disable(ts);
		return 0;
	}
}
static int cxn_open(void)
{
	char w_value[5]={0};
	int ret ;
	//printk(KERN_ERR "eliot cxn_open\n");
	w_value[0] = 1 ;w_value[1] = 0 ;//open cmd
	mutex_lock(&my_cxn_i2c->lock);
	ret = i2c_master_send(my_cxn_i2c->cxn_i2c_client,w_value,2);
	mutex_unlock(&my_cxn_i2c->lock);
	return ret ;
	
}

static void cxn_stop(void)
{
	char w_value[5]={0};
	int ret ;
	//printk(KERN_ERR "eliot cxn_stop\n");
	w_value[0] = 2 ;w_value[1] = 0 ;//stop cmd
	mutex_lock(&my_cxn_i2c->lock);
	ret = i2c_master_send(my_cxn_i2c->cxn_i2c_client,w_value,2);
	mutex_unlock(&my_cxn_i2c->lock);

	//return ret ;
	
}

#if 0 
static void cxn_shutdown(void)
{
	char w_value[5]={0};
	int ret ;
	w_value[0] = 0x0b ;w_value[1] = 1 ;w_value[2] = 0 ;//shutdown cmd
	mutex_lock(&my_cxn_i2c->lock);
	ret = i2c_master_send(my_cxn_i2c->cxn_i2c_client,w_value,3);
	mutex_unlock(&my_cxn_i2c->lock);
}
static void cxn_reboot(void)
{
	char w_value[5]={0};
	int ret ;
	w_value[0] = 0x0b ;w_value[1] = 1 ;w_value[2] = 1 ;//shutdown cmd
	mutex_lock(&my_cxn_i2c->lock);
	ret = i2c_master_send(my_cxn_i2c->cxn_i2c_client,w_value,3);
	mutex_unlock(&my_cxn_i2c->lock);
}
#endif
static int cxn_i2c_resume(struct device *tdev) {
	//printk(KERN_ERR "eliot cxn_i2c_resume my_cxn_i2c->isActive = %d\n",my_cxn_i2c->isActive);
	if(my_cxn_i2c->isActive==0){
		icn_reset_chip();
	}
    return 0;
}

static int cxn_i2c_remove(struct i2c_client *client) {
	
    return 0;
}

static int cxn_i2c_suspend(struct device *tdev) {
	//printk(KERN_ERR "eliot cxn_i2c_suspend my_cxn_i2c->isActive = %d\n",my_cxn_i2c->isActive);
	if(my_cxn_i2c->isActive==1){
		cxn_stop();
		gpio_direction_output(my_cxn_i2c->gpio_rstn, 0);
		my_cxn_i2c->isActive = 0;
	}
    return 0;
}

static ssize_t cxn_ctl_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret ;
	struct cxn_i2c_interface *cxn = dev_get_drvdata(dev);

	sprintf(buf, "%d\n",cxn->isActive);  
  
	ret = strlen(buf) + 1;  
	return ret; 
}

static ssize_t cxn_ctl_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	//int ret ;
	struct cxn_i2c_interface *cxn = dev_get_drvdata(dev);
	
	unsigned long on_off = simple_strtoul(buf, NULL, 10);  

	if(on_off == 1 && cxn->isActive == 0)
	{
		cxn_i2c_resume(&cxn->cxn_i2c_client->dev);
	}
	else if(on_off == 0 && cxn->isActive == 1)
	{
		cxn_i2c_suspend(&cxn->cxn_i2c_client->dev);
		
	}else
	printk(KERN_ERR "eliot :no need do this \n");

	return size;
}

static DEVICE_ATTR(cxn_ctl, 0666,
			cxn_ctl_show,
			cxn_ctl_store);
/*
static DEVICE_ATTR(fw_upgrade, (S_IRUGO | S_IWUSR | S_IWGRP),
			gtp_fw_upgrade_show,
			gtp_fw_upgrade_store);
static DEVICE_ATTR(force_fw_upgrade, (S_IRUGO | S_IWUSR | S_IWGRP),
			gtp_fw_upgrade_show,
			gtp_force_fw_upgrade_store);
*/
static struct attribute *cxn_attrs[] = {
	&dev_attr_cxn_ctl.attr,
	//&dev_attr_fw_upgrade.attr,
	//&dev_attr_force_fw_upgrade.attr,
	NULL
};
static const struct attribute_group cxn_attr_grp = {
	.attrs = cxn_attrs,
};

static void cxn_work_func(struct work_struct *work)
{
	
	int ret ;
	char r_value[5]={0};
	static int count_black = 0 ;
	static int count_normal = 0 ;
	struct cxn_i2c_interface *cxn = container_of(work, struct cxn_i2c_interface, work);
	
	mutex_lock(&cxn->i2c_lock);
	//printk(KERN_ERR "eliot :cxn_irq_handler..start.notify_num= %d\n",cxn->notify_num);
	/*read notify*/
	/*st + slave addr + r + [ack] + [data1] + ack +[data2] +ack +[data3]+ nack +stop */
	
	ret = i2c_master_recv(cxn->cxn_i2c_client,r_value,3);
	//printk(KERN_ERR "eliot: ret = %d,value[0]=%d,value[1]=%d,value[2]=%d\n",ret,r_value[0],r_value[1],r_value[2]);
	if(r_value[0]==0 && r_value[1]==1 && r_value[2]==0)
	{
		cxn_open();
		cxn->isActive = 1 ;//open nomal
	}
	if(r_value[0]==0x11 && r_value[1]==0x01 && r_value[2]==0x80)
	{
		//into mute mode ,thermal protect 60'
		count_black++;
		printk(KERN_ERR "========eliot: into black screen mode %d !!!!======\n",count_black);
	}
	if(r_value[0]==0x11 && r_value[1]==0x01 && r_value[2]==0x0)
	{
		//into unmute mode,return nomal
		count_normal++;
		printk(KERN_ERR "************eliot: change black screen to normal %d*********\n",count_normal);
		
	}
	//printk(KERN_ERR "eliot :cxn_irq_handler.ok..\n");
	mutex_unlock(&cxn->i2c_lock);
	cxn_irq_enable(cxn);
	
}
#if defined(CONFIG_FB)
static int fb_notifier_callback(struct notifier_block *self,
				 unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;
	struct cxn_i2c_interface *cxn =
		container_of(self, struct cxn_i2c_interface, fb_notif);

	if (evdata && evdata->data && event == FB_EVENT_BLANK &&
			cxn && cxn->cxn_i2c_client) {
		blank = evdata->data;
		if (*blank == FB_BLANK_UNBLANK)
			cxn_i2c_resume(&cxn->cxn_i2c_client->dev);
		else if (*blank == FB_BLANK_POWERDOWN)
			cxn_i2c_suspend(&cxn->cxn_i2c_client->dev);
	}

	return 0;
}
#endif


#define I2C_VTG_MIN_UV__	1800000
#define I2C_VTG_MAX_UV__	1800000

static int cxn_i2c_probe(struct i2c_client *client,
        const struct i2c_device_id *id) 
{
	int err = 0 ;
	printk(KERN_ERR "eliot :cxn_i2c_probe start.....");
	
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev,
				"%s: check_functionality failed.", __func__);
		err = -ENODEV;
		goto exit0;
	}

	/* Allocate memory for driver data */
	my_cxn_i2c = kzalloc(sizeof(struct cxn_i2c_interface), GFP_KERNEL);
	
	if (!my_cxn_i2c) {
		dev_err(&client->dev,
				"%s: memory allocation failed.", __func__);
		err = -ENOMEM;
		goto exit1;
	}
	memset(my_cxn_i2c, 0, sizeof(*my_cxn_i2c));
	
	spin_lock_init(&my_cxn_i2c->irq_lock);
	mutex_init(&my_cxn_i2c->lock);
	mutex_init(&my_cxn_i2c->i2c_lock);

	#if defined(CONFIG_FB)
	my_cxn_i2c->fb_notif.notifier_call = fb_notifier_callback;
	err = fb_register_client(&my_cxn_i2c->fb_notif);
	if (err)
		printk(KERN_ERR "eliot :fb_notifier_callback err.....");
	#endif
	
	my_cxn_i2c->notify_num = 0 ;
	my_cxn_i2c->isActive = 0 ;
	
	my_cxn_i2c->cxn_wq = create_singlethread_workqueue("cxn_wq");
	INIT_WORK(&my_cxn_i2c->work, cxn_work_func);
	
	if (!my_cxn_i2c->cxn_wq) {
		printk(KERN_ERR "eliot :Creat workqueue failed.");
		return -ENOMEM;
	}
	if (client->dev.of_node) {
	my_cxn_i2c->gpio_rstn= of_get_named_gpio_flags(client->dev.of_node,
			"cxn-reset-gpios", 0, NULL);
	my_cxn_i2c->gpio_irq= of_get_named_gpio_flags(client->dev.of_node,
			"cxn-interrupt-gpios", 0, NULL);
	}
	else
	{
		printk(KERN_ERR "eliot:client->dev.of_node not exit!\n");
	}

	/***** I2C initialization *****/
	my_cxn_i2c->cxn_i2c_client = client;
	
	/* set client data */
	i2c_set_clientdata(client, my_cxn_i2c);

	/*open l6 1.8 V*/
	
	my_cxn_i2c->vdd = regulator_get(&client->dev, "vdd");
	
	if (IS_ERR(my_cxn_i2c->vdd)) {
		err = PTR_ERR(my_cxn_i2c->vdd);
		printk(KERN_ERR "eliot:vdd get erro err = %d\n",err);
		goto exit1;
	}else
	{
		regulator_set_voltage(my_cxn_i2c->vdd, I2C_VTG_MIN_UV__,
					   I2C_VTG_MAX_UV__);
		err = regulator_enable(my_cxn_i2c->vdd);
		if (err) {
			dev_err(&client->dev,
				"Regulator vdd enable failed ret=%d\n", err);
			goto exit2;
		}
	}
	
	/* initialize pinctrl */
	if (!cxn_pinctrl_init(my_cxn_i2c)) {
		err = pinctrl_select_state(my_cxn_i2c->pinctrl, my_cxn_i2c->pin_default);
		if (err) {
			dev_err(&client->dev, "Can't select pinctrl state\n");
			goto exit2;
		}
	}
	
	mutex_lock(&my_cxn_i2c->lock);

	/* Pull up the reset pin */
	/* request  GPIO  */
	err = gpio_request(my_cxn_i2c->gpio_rstn, "cxn-reset-gpios");
	if (err < 0) {
		printk(KERN_ERR "Failed to request GPIO:%d, ERRNO:%d",
			  my_cxn_i2c->gpio_rstn, err);
		err = -ENODEV;
		mutex_unlock(&my_cxn_i2c->lock);
		goto exit3; 
	}
	err = gpio_request(my_cxn_i2c->gpio_irq, "cxn-interrupt-gpios");
	if (err < 0) {
		printk(KERN_ERR "Failed to request GPIO:%d, ERRNO:%d",
			  my_cxn_i2c->gpio_irq, err);
		gpio_free(my_cxn_i2c->gpio_rstn);
		err = -ENODEV;
		mutex_unlock(&my_cxn_i2c->lock);
		goto exit3; 
	}

	//config irq pin
	err = gpio_direction_input(my_cxn_i2c->gpio_irq);
	if (err < 0)
		gpio_free(my_cxn_i2c->gpio_irq);
	my_cxn_i2c->cxn_i2c_client->irq = gpio_to_irq(my_cxn_i2c->gpio_irq);

	//allocat irq gpio36
	err = cxn_request_irq(my_cxn_i2c);
	if (err < 0){
		printk(KERN_ERR "cxn_request_irq fail !!\n");
		goto exit4 ;
	}
	else
		printk(KERN_ERR "cxn_request_irq works !!!\n");
	
	err = sysfs_create_group(&client->dev.kobj, &cxn_attr_grp);
	if (err < 0) {
		dev_err(&client->dev, "eliot :sys file creation failed.\n");
		goto exit5;
	}

	//reset enable
	gpio_direction_output(my_cxn_i2c->gpio_rstn, 0);
	mdelay(50);
	gpio_direction_output(my_cxn_i2c->gpio_rstn, 1);
	cxn_irq_enable(my_cxn_i2c);

	mutex_unlock(&my_cxn_i2c->lock);
	printk(KERN_ERR "eliot :cxn_i2c_probe end ....\n");
	return 0;
exit5:
	free_irq(client->irq, my_cxn_i2c);
exit4:
	gpio_free(my_cxn_i2c->gpio_rstn);
	gpio_free(my_cxn_i2c->gpio_irq);
exit3:
	mutex_destroy(&my_cxn_i2c->lock);
	mutex_destroy(&my_cxn_i2c->i2c_lock);
exit2:
	regulator_disable(my_cxn_i2c->vdd);
exit1:
	kfree(my_cxn_i2c);
exit0:
	return err ;
}

static const struct of_device_id cxn010x_i2c_of_match[] = {
    { .compatible = "qcom,cxn0102",},
    {},
};

static const struct i2c_device_id cxn_i2c_id[] = {
    {"qcom,cxn0102", 0},
    {},
};


static struct i2c_driver cxn010x_i2c_driver = {
    .driver = {
        .name = "qcom,cxn0102",
        .owner    = THIS_MODULE,
        .of_match_table = cxn010x_i2c_of_match,
        
    },
    .probe    = cxn_i2c_probe,
    .remove   = cxn_i2c_remove,
    .id_table = cxn_i2c_id,
};

module_i2c_driver(cxn010x_i2c_driver);

MODULE_LICENSE("GPL");

