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

#include <linux/fs.h>  
#include <asm/uaccess.h>  
#include <linux/device.h>
#include <linux/cdev.h>


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

	int isOpenedByApp;
	int count_black;
	int count_normal;
	//unsigned long isPower;
};

static struct cxn_i2c_interface *my_cxn_i2c ;

/*****************************************************************/
/*************************cdev************************************/
/*****************************************************************/
#ifdef CXN_I2C_DEV


#define DEVICE_SUM 1

static int cxn_cdev_open(struct inode *inode, struct file *filp);
static int cxn_cdev_release(struct inode *, struct file *filp);
static ssize_t cxn_cdev_read(struct file*, char*, size_t, loff_t*);
static ssize_t cxn_cdev_write(struct file*, const char*, size_t, loff_t*);

/* the major device number */
static int cxn_cdev_major = 0;
static int cxn_cdev_minor = 0;


static struct class* cxn_cdev_class = NULL;
/* define a cdev device */
static struct cdev *cdev;

//static int global_var = 0; /* global var */


/* init the file_operations structure */
struct file_operations cxn_cdev_fops =
{
    .owner = THIS_MODULE,
    .open = cxn_cdev_open,
    .release = cxn_cdev_release,
    .read = cxn_cdev_read,
    .write = cxn_cdev_write,
};



/* open device */
static int cxn_cdev_open(struct inode *inode, struct file *filp)
{
    int ret = 0;
	if( my_cxn_i2c ){
		my_cxn_i2c->isOpenedByApp = 1;
	}
    printk("eliot:cxn_cdev_open success. %d\n", my_cxn_i2c->isOpenedByApp);
    return ret;
}

/* release device */
static int cxn_cdev_release(struct inode *inode, struct file *filp)
{
	if( my_cxn_i2c ){
		my_cxn_i2c->isOpenedByApp = 0;
	}
    printk("eliot:cxn_cdev_release release success. %d\n", my_cxn_i2c->isOpenedByApp);
    return 0;
}

// handle data, which read from bus
static void prism_handle_read( const char* read_buf, size_t read_sz ) {
	if( !my_cxn_i2c ) {
		return;
	}

	// if boot normally
	if(read_buf[0]==0 && read_buf[1]==1 && read_buf[2]==0)
	{
		char w_value[2]={0x01,0x00}; // start input
		int ret ;

		ret = i2c_master_send(my_cxn_i2c->cxn_i2c_client,w_value,sizeof(w_value));
		my_cxn_i2c->isActive = 1 ;//open nomal
	}

	if(read_buf[0]==0x11 && read_buf[1]==0x01 && read_buf[2]==0x80)
	{
		//into mute mode ,thermal protect 60'
		my_cxn_i2c->count_black++;
		printk(KERN_ERR "========eliot: into black screen mode %d !!!!======\n",my_cxn_i2c->count_black);
	}
	if(read_buf[0]==0x11 && read_buf[1]==0x01 && read_buf[2]==0x0)
	{
		//into unmute mode,return nomal
		my_cxn_i2c->count_normal++;
		printk(KERN_ERR "************eliot: change black screen to normal %d*********\n",my_cxn_i2c->count_normal);
	}

	return;
};

// handle data, which will write to bus

static void prism_handle_write( const char* write_buf, size_t write_sz ) {
	return;
};



#define BUF_SIZE 1024
/* read device */

static ssize_t cxn_cdev_read(struct file *filp, char *buf, size_t len, loff_t *off)
{
	char buffer[BUF_SIZE+1] = {0};
	int n_read = 0;
	int step = 0;
	int ret = 0;

	mutex_lock(&my_cxn_i2c->lock);

	while( n_read < len ) {
		// calculate ecpect read size, should not more than left space of user, to make should no waste
		step = BUF_SIZE;
		if( step + n_read > len ) {
			step = len - n_read;
		}

		// read from bus
		ret = i2c_master_recv(my_cxn_i2c->cxn_i2c_client,buffer,step);
		if( ret <= 0 ) {
			break;
		}

		// prism handle
		prism_handle_read(buffer, ret);

		// save actually read to user buffer
		ret = copy_to_user(buf, buffer, ret);
		if( ret != 0 ) {
			printk("thir: Error to copy to user data: %d.\n", ret);
		}
		n_read += step;
	}

	mutex_unlock(&my_cxn_i2c->lock);
    return n_read;
}

/* write device */
static ssize_t cxn_cdev_write(struct file *filp, const char *buf, size_t len, loff_t *off)
{
    char buffer[BUF_SIZE+1]={0};
	int n_write = 0;
	int step = 0;
	int ret = 0;

	//printk(KERN_ERR "eliot :cxn_cdev_write writing...\n");
	mutex_lock(&my_cxn_i2c->lock);
	while( n_write < len ) {
		// calculate ecpect write size, if data too long, will be separated
		step = ((len - n_write) < BUF_SIZE)?(len - n_write):BUF_SIZE;

		// get data from user buffer
		ret = copy_from_user(buffer, buf+n_write,  step);
		if( ret != 0 ) {
			printk("thir: Error to copy user data: %d.\n", ret);
		}

		// prism handle
		prism_handle_write(buffer, ret);

		// send data
		i2c_master_send(my_cxn_i2c->cxn_i2c_client,buffer,step);
		n_write += step;
	}
	mutex_unlock(&my_cxn_i2c->lock);
    return n_write;//sizeof(int);
}
#endif
/*****************************************************************/
/*********************cdev****************************************/
/*****************************************************************/

/*****************************************************************/
/*********************input dev************************************/
/*****************************************************************/

static struct input_dev  *cxn_input_dev;

/*****************************************************************/
/*********************input dev************************************/
/*****************************************************************/

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
/*
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
*/

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
#define READ_SZ 32
	int ret = 0;
	char r_value_buff[READ_SZ]={0};
	//char r_value[35]={0};

	struct cxn_i2c_interface *cxn = container_of(work, struct cxn_i2c_interface, work);
	
	mutex_lock(&cxn->i2c_lock);
	//printk(KERN_ERR "eliot :cxn_irq_handler..start.notify_num= %d\n",cxn->notify_num);
	/*read notify*/
	/*st + slave addr + r + [ack] + [data1] + ack +[data2] +ack +[data3]+ nack +stop */
	memset(r_value_buff, 0xff, sizeof(r_value_buff));//add by eliot shao 2017.2.27	

	//printk(KERN_ERR "thir: cxn_work_func isOpenedByApp=%d\n" , my_cxn_i2c->isOpenedByApp);
	// by thirchina auto read when no app working
	if( !my_cxn_i2c || my_cxn_i2c->isOpenedByApp == 0){
		ret = i2c_master_recv(cxn->cxn_i2c_client,r_value_buff,READ_SZ);
		//printk(KERN_ERR "thir: ret = %d,value[0]=%d,value[1]=%d,value[2]=%d\n",ret,r_value_buff[0],r_value_buff[1],r_value_buff[2]);

		prism_handle_read(r_value_buff, ret);
	}

	if( ret >= 0 )
	{
		printk(KERN_ERR "thir: notify app\n");
		input_event(cxn_input_dev, EV_MSC, MSC_RAW, 0xfe);
		input_sync(cxn_input_dev);
	}

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

#ifdef CXN_I2C_DEV
	int ret = 0 ;
	struct device* temp = NULL;
	dev_t devno = 0;
	ret= alloc_chrdev_region(&devno, cxn_cdev_minor, DEVICE_SUM, "cxn010x");
    if(ret < 0) {
        printk(KERN_ALERT "cxn_cdev:Failed to alloc char dev region.\n");
        goto exit0;
    }
	
	cxn_cdev_major = MAJOR(devno);
	cxn_cdev_minor = MINOR(devno);
	cdev = cdev_alloc();
    cdev->owner = THIS_MODULE;
    cdev->ops = &cxn_cdev_fops;
    if ((ret = cdev_add(cdev, devno, 1)))
    {
        printk(KERN_NOTICE "cxn_cdev:Error %d adding cxn_cdev.\n", ret);
        goto destroy_cdev;
    }
    else
        printk(KERN_ALERT"cxn_cdev:cxn_cdev register success.\n");
        /*在/sys/class/目录下创建设备类别目录*/
    cxn_cdev_class = class_create(THIS_MODULE, "cxn010x");
    if(IS_ERR(cxn_cdev_class)) {
        ret = PTR_ERR(cxn_cdev_class);
        printk(KERN_ALERT"Failed to create cxn_cdev class.\n");
        goto destroy_cdev;
    }        
    /*在/dev/目录和/sys/class/cxn_cdev目录下分别创建设备文件*/
    temp = device_create(cxn_cdev_class, NULL, devno, "%s", "cxn010x");
    if(IS_ERR(temp)) {
        ret = PTR_ERR(temp);
        printk(KERN_ALERT"Failed to create cxn_cdev device.");
        goto destroy_class;
    }    
#endif
#ifdef CXN_INPUT
	cxn_input_dev = input_allocate_device();
	if (cxn_input_dev == NULL) {
		printk(KERN_ERR"eliot Failed to allocate input device.\n");
		return -ENOMEM;
	}
	cxn_input_dev->evbit[0] = BIT_MASK(EV_MSC) ;
	
	input_set_capability(cxn_input_dev, EV_MSC, MSC_RAW);

	cxn_input_dev->name = "cxn010x";
	cxn_input_dev->phys = "cxn010x/input0";
	cxn_input_dev->id.bustype = BUS_I2C;
	cxn_input_dev->id.vendor = 0xDEAD;
	cxn_input_dev->id.product = 0xBEEF;
	cxn_input_dev->id.version = 10;

	err = input_register_device(cxn_input_dev);
	if (err) {
		printk(KERN_ERR"eliot:Register %s input device failed.\n",cxn_input_dev->name);
		goto exit_free_inputdev;
	}else
	printk(KERN_ERR"eliot:Register %s input device register success.\n",cxn_input_dev->name);
	
#endif


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
	mdelay(150);
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
#ifdef CXN_I2C_DEV
destroy_class:
	class_destroy(cxn_cdev_class);
destroy_cdev:
	cdev_del(cdev);
#endif
#ifdef CXN_INPUT
exit_free_inputdev:
	input_free_device(cxn_input_dev);
	cxn_input_dev = NULL;
#endif
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
MODULE_AUTHOR("eliot shao");


