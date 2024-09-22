// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>
#include <linux/of.h>
#include <linux/module.h>

#include "tps65132.h"

/* I2C Slave Setting */
#define tps65132_SLAVE_ADDR_WRITE	0x3E

static struct i2c_client *new_client;
static bool ocp2131_status = false;
static const struct i2c_device_id tps65132_i2c_id[] = { {"tps65132", 0}, {} };

static int tps65132_driver_probe(struct i2c_client *client,
	const struct i2c_device_id *id);
static int tps65132_driver_remove(struct i2c_client *client);

#ifdef CONFIG_OF
static const struct of_device_id tps65132_id[] = {
	{.compatible = "mediatek,i2c_lcd_bias"},
	{},
};

MODULE_DEVICE_TABLE(of, tps65132_id);
#endif

static struct i2c_driver tps65132_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "tps65132",
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(tps65132_id),
#endif
	},
	.probe = tps65132_driver_probe,
	.remove = tps65132_driver_remove,
	.id_table = tps65132_i2c_id,
};

static DEFINE_MUTEX(tps65132_i2c_access);

/* I2C Function For Read/Write */
int tps65132_read_byte(unsigned char cmd, unsigned char *returnData)
{
	char cmd_buf[2] = { 0x00, 0x00 };
	char readData = 0;
	int ret = 0;

	mutex_lock(&tps65132_i2c_access);

	cmd_buf[0] = cmd;
	ret = i2c_master_send(new_client, &cmd_buf[0], 1);
	ret = i2c_master_recv(new_client, &cmd_buf[1], 1);
	if (ret < 0) {
		mutex_unlock(&tps65132_i2c_access);
		return 0;
	}

	readData = cmd_buf[1];
	*returnData = readData;

	mutex_unlock(&tps65132_i2c_access);

	return 1;
}
EXPORT_SYMBOL(tps65132_read_byte);

int tps65132_write_byte(unsigned char cmd, unsigned char writeData)
{
	char write_data[2] = { 0 };
	int ret = 0;

	pr_info("[KE/tps65132] cmd: %02x, data: %02x,%s\n", cmd, writeData, __func__);

	if (ocp2131_status == false) {
		pr_info("[KE/tps65132] tps65132 status error %s\n", __func__);
		return 0;
	}
	mutex_lock(&tps65132_i2c_access);

	write_data[0] = cmd;
	write_data[1] = writeData;

	ret = i2c_master_send(new_client, write_data, 2);
	if (ret < 0) {
		mutex_unlock(&tps65132_i2c_access);
		pr_info("[tps65132] I2C write fail!!!\n");

		return 0;
	}

	mutex_unlock(&tps65132_i2c_access);

	return 1;
}
EXPORT_SYMBOL(tps65132_write_byte);

static int tps65132_driver_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int err = 0;

	pr_info("[KE/tps65132] name=%s addr=0x%x\n",
		client->name, client->addr);
	new_client = kmalloc(sizeof(struct i2c_client), GFP_KERNEL);
	if (!new_client) {
		err = -ENOMEM;
		goto exit;
	}

	memset(new_client, 0, sizeof(struct i2c_client));
	new_client = client;
	ocp2131_status = true;

	return 0;

 exit:
	return err;

}

static int tps65132_driver_remove(struct i2c_client *client)
{
	pr_info("[KE/tps65132] %s\n", __func__);

	new_client = NULL;
	i2c_unregister_device(client);

	return 0;
}

#define tps65132_BUSNUM 6

static int __init tps65132_init(void)
{
	pr_info("[KE/tps65132] %s\n", __func__);

	if (i2c_add_driver(&tps65132_driver) != 0)
		pr_info("[KE/tps65132] failed to register tps65132 i2c driver.\n");
	else
		pr_info("[KE/tps65132] Success to register tps65132 i2c driver.\n");

	return 0;
}

static void __exit tps65132_exit(void)
{
	i2c_del_driver(&tps65132_driver);
}

module_init(tps65132_init);
module_exit(tps65132_exit);

MODULE_AUTHOR("Mediatek Corporation");
MODULE_DESCRIPTION("tps65132 Driver");
MODULE_LICENSE("GPL");
