/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _tps65132_SW_H_
#define _tps65132_SW_H_

#ifndef BUILD_LK
struct tps65132_setting_table {
	unsigned char cmd;
	unsigned char data;
};

extern int tps65132_read_byte(unsigned char cmd, unsigned char *returnData);
extern int tps65132_write_byte(unsigned char cmd, unsigned char writeData);
#endif

#endif
