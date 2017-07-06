/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
   ocom,cont-splash-enabled;
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

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include "msm_dba_internal.h"
#include <linux/mdss_io_util.h>

#define ADV7533_REG_CHIP_REVISION (0x00)
#define ADV7533_DSI_CEC_I2C_ADDR_REG (0xE1)
#define ADV7533_RESET_DELAY (10)

#define PINCTRL_STATE_ACTIVE    "pmx_fake_dba_active"
#define PINCTRL_STATE_SUSPEND   "pmx_fake_dba_suspend"

#define MDSS_MAX_PANEL_LEN      256
#define EDID_SEG_SIZE 0x100
/* size of audio and speaker info Block */
#define AUDIO_DATA_SIZE 32

/* 0x94 interrupts */
#define HPD_INT_ENABLE           BIT(7)
#define MONITOR_SENSE_INT_ENABLE BIT(6)
#define ACTIVE_VSYNC_EDGE        BIT(5)
#define AUDIO_FIFO_FULL          BIT(4)
#define EDID_READY_INT_ENABLE    BIT(2)

#define MAX_WAIT_TIME (100)
#define MAX_RW_TRIES (3)

/* 0x95 interrupts */
#define CEC_TX_READY             BIT(5)
#define CEC_TX_ARB_LOST          BIT(4)
#define CEC_TX_RETRY_TIMEOUT     BIT(3)
#define CEC_TX_RX_BUF3_READY     BIT(2)
#define CEC_TX_RX_BUF2_READY     BIT(1)
#define CEC_TX_RX_BUF1_READY     BIT(0)

#define HPD_INTERRUPTS           (HPD_INT_ENABLE | \
		MONITOR_SENSE_INT_ENABLE)
#define EDID_INTERRUPTS          EDID_READY_INT_ENABLE
#define CEC_INTERRUPTS           (CEC_TX_READY | \
		CEC_TX_ARB_LOST | \
		CEC_TX_RETRY_TIMEOUT | \
		CEC_TX_RX_BUF3_READY | \
		CEC_TX_RX_BUF2_READY | \
		CEC_TX_RX_BUF1_READY)

#define CFG_HPD_INTERRUPTS       BIT(0)
#define CFG_EDID_INTERRUPTS      BIT(1)
#define CFG_CEC_INTERRUPTS       BIT(3)

#define MAX_OPERAND_SIZE	14
#define CEC_MSG_SIZE            (MAX_OPERAND_SIZE + 2)

enum fake_dba_i2c_addr {
	I2C_ADDR_MAIN = 0x3D,
	I2C_ADDR_CEC_DSI = 0x3C,
};

enum fake_dba_cec_buf {
	ADV7533_CEC_BUF1,
	ADV7533_CEC_BUF2,
	ADV7533_CEC_BUF3,
	ADV7533_CEC_BUF_MAX,
};

struct fake_dba_reg_cfg {
	u8 i2c_addr;
	u8 reg;
	u8 val;
	int sleep_in_ms;
};

struct fake_dba_cec_msg {
	u8 buf[CEC_MSG_SIZE];
	u8 timestamp;
	bool pending;
};

struct fake_dba {
	u8 main_i2c_addr;
	u8 cec_dsi_i2c_addr;
	u8 video_mode;
	int irq;
	u32 irq_gpio;
	u32 irq_flags;
	u32 hpd_irq_gpio;
	u32 hpd_irq_flags;
	u32 switch_gpio;
	u32 switch_flags;
	struct pinctrl *ts_pinctrl;
	struct pinctrl_state *pinctrl_state_active;
	struct pinctrl_state *pinctrl_state_suspend;
	bool audio;
	bool fake_brige;
	bool disable_gpios;
	struct dss_module_power power_data;
	bool cec_enabled;
	bool is_power_on;
	void *edid_data;
	u8 edid_buf[EDID_SEG_SIZE];
	u8 audio_spkr_data[AUDIO_DATA_SIZE];
	struct workqueue_struct *workq;
	struct delayed_work fake_dba_intr_work_id;
	struct msm_dba_device_info dev_info;
	struct fake_dba_cec_msg cec_msg[ADV7533_CEC_BUF_MAX];
	struct i2c_client *i2c_client;
	struct mutex ops_mutex;
};

static char mdss_mdp_panel[MDSS_MAX_PANEL_LEN];


static void fake_dba_notify_clients(struct msm_dba_device_info *dev,
		enum msm_dba_callback_event event)
{
	struct msm_dba_client_info *c;
	struct list_head *pos = NULL;

	if (!dev) {
		pr_err("%s: invalid input\n", __func__);
		return;
	}

	list_for_each(pos, &dev->client_list) {
		c = list_entry(pos, struct msm_dba_client_info, list);

		pr_debug("%s: notifying event %d to client %s\n", __func__,
				event, c->client_name);

		if (c && c->cb)
			c->cb(c->cb_data, event);
	}
}


static struct i2c_device_id fake_dba_id[] = {
	{ "fake-brige", 0},
	{}
};


static int fake_dba_register_dba(struct fake_dba *pdata)
{
	struct msm_dba_ops *client_ops;
	struct msm_dba_device_ops *dev_ops;

	if (!pdata)
		return -EINVAL;

	client_ops = &pdata->dev_info.client_ops;
	dev_ops = &pdata->dev_info.dev_ops;

	strlcpy(pdata->dev_info.chip_name, "fake-dba",
			sizeof(pdata->dev_info.chip_name));

	mutex_init(&pdata->dev_info.dev_mutex);

	INIT_LIST_HEAD(&pdata->dev_info.client_list);

	return msm_dba_add_probed_device(&pdata->dev_info);
}

static void fake_dba_unregister_dba(struct fake_dba *pdata)
{
	if (!pdata)
		return;

	msm_dba_remove_probed_device(&pdata->dev_info);
}

/*
 *  Also to see function static void fake_dba_intr_work(struct work_struct *work);
 *   */
static void fake_brige_intr_work(struct work_struct *work)
{
	struct fake_dba *pdata;
	struct delayed_work *dw = to_delayed_work(work);

	pr_err("mickey, %s,%d\n",__func__,__LINE__);

	pdata = container_of(dw, struct fake_dba,
			fake_dba_intr_work_id);
	if (!pdata) {
		pr_err("%s: invalid input\n", __func__);
		return;
	}
	//here we have to fake an EDID for DSI1
	//TODO...

	fake_dba_notify_clients(&pdata->dev_info,MSM_DBA_CB_HPD_CONNECT);
}

static int fake_dba_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	static struct fake_dba *pdata;
	int ret = 0;

	if (!client || !client->dev.of_node) {
		pr_err("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	pdata = devm_kzalloc(&client->dev,
			sizeof(struct fake_dba), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	pdata->i2c_client = client;
	pdata->dev_info.instance_id = 0;
	/*Added by mickey.shi, 11/8/2016*/
	pr_err("%s,%d,we are now in fake brige mode!\n",__func__,__LINE__);

	mutex_init(&pdata->ops_mutex);

	ret = fake_dba_register_dba(pdata);
	if (ret) {
		pr_err("%s: Error registering with DBA %d\n",
				__func__, ret);
		goto err_dba_reg;
	}

	ret = msm_dba_helper_sysfs_init(&client->dev);
	if (ret) {
		pr_err("%s: sysfs init failed\n", __func__);
		goto err_dba_reg;
	}

	pdata->workq = create_workqueue("fake_dba_workq");
	if (!pdata->workq) {
		pr_err("%s: workqueue creation failed.\n", __func__);
		ret = -EPERM;
		goto err_workqueue;
	}

	INIT_DELAYED_WORK(&pdata->fake_dba_intr_work_id, fake_brige_intr_work);

	queue_delayed_work(pdata->workq, &pdata->fake_dba_intr_work_id, msecs_to_jiffies(20000));

	pm_runtime_enable(&client->dev);
	pm_runtime_set_active(&client->dev);

	return 0;

err_workqueue:
	msm_dba_helper_sysfs_remove(&client->dev);
	fake_dba_unregister_dba(pdata);
err_dba_reg:
	devm_kfree(&client->dev, pdata);
	return ret;
}

static int fake_dba_remove(struct i2c_client *client)
{
	int ret = -EINVAL;
	struct msm_dba_device_info *dev;
	struct fake_dba *pdata;

	if (!client)
		goto end;

	dev = dev_get_drvdata(&client->dev);
	if (!dev)
		goto end;

	pdata = container_of(dev, struct fake_dba, dev_info);
	if (!pdata)
		goto end;

	pm_runtime_disable(&client->dev);
	disable_irq(pdata->irq);
	free_irq(pdata->irq, pdata);

	mutex_destroy(&pdata->ops_mutex);

	devm_kfree(&client->dev, pdata);

end:
	return ret;
}

static struct i2c_driver fake_dba_driver = {
	.driver = {
		.name = "fake-brige",
		.owner = THIS_MODULE,
	},
	.probe = fake_dba_probe,
	.remove = fake_dba_remove,
	.id_table = fake_dba_id,
};

static int __init fake_dba_init(void)
{
	return i2c_add_driver(&fake_dba_driver);
}

static void __exit fake_dba_exit(void)
{
	i2c_del_driver(&fake_dba_driver);
}

module_param_string(panel, mdss_mdp_panel, MDSS_MAX_PANEL_LEN, 0);

module_init(fake_dba_init);
module_exit(fake_dba_exit);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("fake_dba driver");
