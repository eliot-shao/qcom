/*
eliot shao 2016/11/24
for icn6202 mipi to lvds bridge
*/

#include <i2c_qup.h>
#include <blsp_qup.h>

#include <debug.h>
#include <err.h>
#include <mipi_dsi.h>
#include <boot_stats.h>
#include <platform/gpio.h>
#include <msm_panel.h>
#include <mipi_dsi.h>
#include <board.h>
#include <mdp5.h>
#include <scm.h>
#include <platform/iomap.h>

static struct qup_i2c_dev  *i2c_dev_6202;
#define  Slave_addr   0x2D

static int qrd_icn_i2c_read(uint8_t addr)  
{  
    int ret = 0; 
	int val = 0 ;
    /* Create a i2c_msg buffer, that is used to put the controller into read 
       mode and then to read some data. */  
    struct i2c_msg msg_buf[] = {  
        {Slave_addr, I2C_M_WR, 1, &addr},  
        {Slave_addr, I2C_M_RD, 1, &val}  
    };  
  
    ret = qup_i2c_xfer(i2c_dev_6202, msg_buf, 2);
	dprintf(CRITICAL, "eliot qrd_lcd_i2c_read addr = %d, val = 0x%x\n",addr,val);
    if(ret < 0) {  
        dprintf(CRITICAL, "qup_i2c_xfer error %d\n", ret);  
        return ret;  
    }  
    return val;  
}  
static int qrd_icn_i2c_write(uint8_t addr, uint8_t val)
{
	int ret = 0;
	uint8_t data_buf[] = { addr, val };

	/* Create a i2c_msg buffer, that is used to put the controller into write
	   mode and then to write some data. */
	struct i2c_msg msg_buf[] = { {Slave_addr,
				      I2C_M_WR, 2, data_buf}
	};

	ret = qup_i2c_xfer(i2c_dev_6202, msg_buf, 1);
	if(ret < 0) {
		dprintf(CRITICAL, "eliot qup_i2c_xfer error %d\n", ret);
		return ret;
	}
	return 0;
}

int RESET_chip()
{
	/* configure REST gpio */
	gpio_tlmm_config(25, 0, GPIO_OUTPUT, GPIO_NO_PULL,
			GPIO_8MA, 1);//rst

	gpio_set_dir(25,2);

	dprintf(CRITICAL, "eliot:RESET_chip\n");
	return 0;
}
	/*add by eliot shao 2016.11.25*/
	/*resquest i2c5 and send cmds to icn6202 */
	/*and how to request an i2c in qcom ? follow it !!*/
int LVDS_Init()
{
	dprintf(CRITICAL, "eliot:LVDS_Init");
	//int i = 0;
	RESET_chip();

  	i2c_dev_6202 = qup_blsp_i2c_init(BLSP_ID_1, QUP_ID_4, 100000, 19200000);
	if(!i2c_dev_6202) {
 		 dprintf(CRITICAL, "eliot:qup_blsp_i2c_init failed \n");
  		return 0;
	}

	qrd_icn_i2c_read(0);
	qrd_icn_i2c_write(0x20,0x0);
	qrd_icn_i2c_write(0x21,0x58);
	qrd_icn_i2c_write(0x22,0x24);
	qrd_icn_i2c_write(0x23,0x96);
	qrd_icn_i2c_write(0x24, 0x14);
	qrd_icn_i2c_write(0x25, 0x96);
	qrd_icn_i2c_write(0x26, 0x0);
	qrd_icn_i2c_write(0x27, 0x0F);
	qrd_icn_i2c_write(0x28, 0x05);
	qrd_icn_i2c_write(0x29, 0x0F);
	qrd_icn_i2c_write(0x34, 0x80);
	qrd_icn_i2c_write(0x36, 0x96);
	qrd_icn_i2c_write(0xB5, 0xA0);
	qrd_icn_i2c_write(0x5C, 0xFF);
	qrd_icn_i2c_write(0x13, 0x10);
	qrd_icn_i2c_write(0x56, 0x90);
	qrd_icn_i2c_write(0x6B, 0x21);
	qrd_icn_i2c_write(0x69, 0x1C);
	qrd_icn_i2c_write(0xB6, 0x20);
	qrd_icn_i2c_write(0x51, 0x20);
	qrd_icn_i2c_write(0x09, 0x10);		
	dprintf(CRITICAL, "eliot:icn6202 i2c init OK !!!! \n");
	
}