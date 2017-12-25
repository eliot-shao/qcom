/* 
 * Author: The Walking Dead
 *
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
#include <linux/input/mt.h>

#include <linux/fs.h>  
#include <asm/uaccess.h>  
#include <linux/device.h>
#include <linux/cdev.h>


#include <linux/pinctrl/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/notifier.h>
#include <linux/fb.h>

struct virtual_tp {
	dev_t devno ;
	struct device* temp;
	struct class* cdev_class ;
	struct cdev* cdev;
	int cdev_major;
	int cdev_minor;
	
	struct input_dev  *vtp_input_dev ;
};

static struct virtual_tp* vtp ;
/* open device */
static int vtp_cdev_open(struct inode *inode, struct file *filp)
{
    int ret = 0;
	printk(KERN_ALERT "vtp_cdev:vtp_cdev_open\n");
    return ret;
}

#define UPDATE_COORDINATE  0x9981
#define NUM_COPY_INT 2
static long vtp_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	int data[] = {0,0} ;
	int __user *argp = (int __user *)arg;
	if (!argp)
		return -EINVAL;
	switch (cmd) {
	case UPDATE_COORDINATE:
		//printk(KERN_ERR "eliot vtp_ioctl UPDATE_COORDINATE\n");
		if (copy_from_user(data, argp, NUM_COPY_INT*sizeof(int)))
			return -EFAULT;
		//printk(KERN_ERR "eliot copy data[0]=%d,data[1]=%d\n",data[0],data[1]);
		if(vtp->vtp_input_dev){
		//input_report_abs(vtp->vtp_input_dev, ABS_X, data[0]);
		//input_report_abs(vtp->vtp_input_dev, ABS_Y, data[1]);
		input_mt_report_slot_state(vtp->vtp_input_dev, MT_TOOL_FINGER, true);
		input_report_abs(vtp->vtp_input_dev, ABS_MT_POSITION_X, data[0]);
		input_report_abs(vtp->vtp_input_dev, ABS_MT_POSITION_Y, data[1]);
		input_report_abs(vtp->vtp_input_dev, ABS_MT_TOUCH_MAJOR, 150);
		//input_report_key(vtp->vtp_input_dev, BTN_TOUCH, 1);
		input_sync(vtp->vtp_input_dev);
		mdelay(20);
		//input_report_key(vtp->vtp_input_dev, BTN_TOUCH, 0);
		input_mt_report_slot_state(vtp->vtp_input_dev, MT_TOOL_FINGER, false);
		input_sync(vtp->vtp_input_dev);
		
		}
		break;
	default:
		break;
	}

	return ret;
}
struct file_operations vtp_cdev_fops = {
	.owner = THIS_MODULE,
    .open = vtp_cdev_open,
	.unlocked_ioctl = vtp_ioctl,

};

static int __init virtTp_init(void)
{
	int ret = 0 ;
	printk(KERN_ERR "eliot virtTp_init...start\n");
	vtp = kmalloc(sizeof(*vtp),GFP_KERNEL);
	memset(vtp,0,sizeof(*vtp));
	ret= alloc_chrdev_region(&vtp->devno, vtp->cdev_minor, 1, "vtp"); 
	if(ret < 0) 
	{
		printk(KERN_ALERT "vtp_cdev:Failed to alloc char dev region.\n");
		goto exit0;    
	}
	vtp->cdev_major= MAJOR(vtp->devno);
	vtp->cdev_minor = MINOR(vtp->devno);
	vtp->cdev = cdev_alloc();
    vtp->cdev->owner = THIS_MODULE;
    vtp->cdev->ops = &vtp_cdev_fops;
	
	if ((ret = cdev_add(vtp->cdev, vtp->devno, 1)))
    {
        printk(KERN_NOTICE "vtp_cdev:Error %d adding ctp_cdev.\n", ret);
        goto exit1;
    } else printk(KERN_ALERT "vtp_cdev:vtp_cdev register success.\n");
	
	vtp->cdev_class= class_create(THIS_MODULE, "vtp");
    if(IS_ERR(vtp->cdev_class)) {
        ret = PTR_ERR(vtp->cdev_class);
        printk(KERN_ALERT "Failed to create vtp_cdev class.\n");
        goto exit1;
    }        
    vtp->temp= device_create(vtp->cdev_class, NULL, vtp->devno, "%s", "vtp");
    if(IS_ERR(vtp->temp)) {
        ret = PTR_ERR(vtp->temp);
        printk(KERN_ALERT"Failed to create vtp_cdev device.");
        goto exit2;
    }   
	
	vtp->vtp_input_dev = input_allocate_device();

	if (vtp->vtp_input_dev == NULL) {
		printk(KERN_ERR "eliot Failed to allocate input device.\n");
		return -ENOMEM;
	}
	input_set_drvdata(vtp->vtp_input_dev, vtp);
	
	vtp->vtp_input_dev->evbit[0] = BIT_MASK(EV_ABS) | BIT_MASK(EV_KEY)| BIT_MASK(EV_SYN);
	input_set_capability(vtp->vtp_input_dev, EV_KEY, BTN_TOUCH);
	
	input_set_abs_params(vtp->vtp_input_dev, ABS_X, 0, 1024, 0, 0);
	input_set_abs_params(vtp->vtp_input_dev, ABS_Y, 0, 600, 0, 0);
	input_set_abs_params(vtp->vtp_input_dev, ABS_MT_POSITION_X,
			     0, 1024, 0, 0);
	input_set_abs_params(vtp->vtp_input_dev, ABS_MT_POSITION_Y,
			     0, 600, 0, 0);
	input_set_abs_params(vtp->vtp_input_dev, ABS_MT_TOUCH_MAJOR,
			     0, 255, 0, 0);
	input_set_abs_params(vtp->vtp_input_dev,ABS_MT_WIDTH_MAJOR,
				0, 200, 0, 0);
	input_set_abs_params(vtp->vtp_input_dev, ABS_MT_TRACKING_ID,0,5,0,0);
	
	vtp->vtp_input_dev->name = "vtp";
	//vtp->vtp_input_dev->phys = "vtp/input0";
	vtp->vtp_input_dev->id.bustype = BUS_VIRTUAL;
	vtp->vtp_input_dev->id.vendor = 0xDEAD;
	vtp->vtp_input_dev->id.product = 0xBEEF;
	vtp->vtp_input_dev->id.version = 10;

	ret = input_register_device(vtp->vtp_input_dev);
	if (ret) {
		printk(KERN_ERR"eliot:Register %s input device failed.\n",vtp->vtp_input_dev->name);
		goto exit3;
	}
	else
		printk(KERN_ERR "eliot:Register %s input device register success.\n",vtp->vtp_input_dev->name);

	printk(KERN_ERR "eliot virtTp_init...end\n");
	return ret;
	
exit3:
	input_free_device(vtp->vtp_input_dev);
exit2:
	class_destroy(vtp->cdev_class);
exit1:
	cdev_del(vtp->cdev);
exit0:
	kfree(vtp);
	return ret ;

}

static void __exit virtTp_exit(void)
{
	cdev_del(vtp->cdev);
	class_destroy(vtp->cdev_class);
	kfree(vtp);
}

module_init(virtTp_init);
module_exit(virtTp_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("The Walking Dead");
