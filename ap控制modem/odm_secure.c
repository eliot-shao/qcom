#include <linux/clk.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/module.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
//#include <mach/board.h>
#include <linux/mutex.h>
#include <linux/timer.h>
#include <linux/remote_spinlock.h>
#include <linux/gpio.h>
//#include <mach/mpp.h>
//#include <mach/proc_comm.h>
#include <soc/qcom/smsm.h>
#include <linux/sched.h>
#include <linux/of_gpio.h>

#include <linux/fs.h>
#include <linux/regulator/consumer.h>


#define LED_START       0x3010
#define LED_STOP        0x3011
#define SMSM_VENDOR      0x00020000

struct simio_imessage
{
  unsigned int cmd;
  unsigned char argv[8];
};
struct simio_imessage * pimsg = NULL;

static ssize_t simcmd_write(int i)
{
	 pr_err("guowei:KFC111111111!!!\n");
	 switch(i)
	 {	
		case 1:
		    pimsg->cmd = LED_START;
			break;
			
		case 2:
			pimsg->cmd = LED_STOP;
			break;
	 }
	 //modem_callback_done =0;
     if(smsm_get_state(SMSM_APPS_STATE) & SMSM_VENDOR)
     {
        smsm_change_state(SMSM_APPS_STATE, SMSM_VENDOR, 0);
        printk("simio_write:smsm_change_state clr bit\r\n");
     }
     else
     {
        smsm_change_state(SMSM_APPS_STATE ,0, SMSM_VENDOR);
        printk("simio_write:smsm_change_state set bit\r\n");
     }
	 return 0;
}

static ssize_t enable_store(struct device *dev,struct device_attribute *attr,const char *buf, size_t size)
{
	if(buf[0] == '1') {
		
		simcmd_write(1);
	  
	} else if(buf[0] == '0'){

		simcmd_write(0);
		
	} else if(buf[0] == '2'){
		
		simcmd_write(2);
		
	}else if(buf[0] == '3'){
		
		simcmd_write(3);
		
	}else if(buf[0] == '4'){
		
		simcmd_write(4);
	}else if(buf[0] == '5'){
		
		simcmd_write(5);
	}else if(buf[0] == '6'){
		
		simcmd_write(6);
	}else if(buf[0] == '7'){
		
		simcmd_write(7);
	}else if(buf[0] == '8'){
		
		simcmd_write(8);
	}else if(buf[0] == '9'){
		
		simcmd_write(9);
	}
	else {
		pr_err("%s bad para!!!\n",__func__);
	}
	return strlen(buf);
}

static DEVICE_ATTR(enable, 0644/*S_IWUSR | S_IWGRP*/, NULL, enable_store);

static ssize_t simio_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{
	return 0;
}

static const struct file_operations simio_fops = {
	.owner = THIS_MODULE,
	.read = simio_read,
};

static struct miscdevice simio_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "simio",
	.fops = &simio_fops,
};

int __init get_serialno_init(void)  
{
	int err = 0;
	printk("get_serialno init\n");
	err = misc_register(&simio_device);
	if (err) {
		printk(KERN_ERR "simio device register failed\n");
	}
	
	pimsg = smem_alloc(SMEM_ID_VENDOR1, sizeof(struct simio_imessage), 0, SMEM_ANY_HOST_FLAG);
    if(pimsg==NULL)
    {
        pr_err("%s:smem_alloc mem failed!\n",__func__);
    }
	device_create_file(simio_device.this_device, &dev_attr_enable);
	
    return 0;
}

void __exit get_serialno_exit(void)
{
    printk("get_serialno exit\n");
}
 
late_initcall(get_serialno_init);
module_exit(get_serialno_exit);
 
MODULE_LICENSE("GPL");

