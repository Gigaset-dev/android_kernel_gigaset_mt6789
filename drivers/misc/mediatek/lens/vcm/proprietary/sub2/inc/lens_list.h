/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */



#ifndef _LENS_LIST_H

#define _LENS_LIST_H


//drv added ,wangmd, support DW9718SAF,20240705 start
extern int DW9718SAF_SetI2Cclient_Sub2(struct i2c_client *pstAF_I2Cclient, spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long DW9718SAF_Ioctl_Sub2(struct file *a_pstFile, unsigned int a_u4Command, unsigned long a_u4Param);
extern int DW9718SAF_Release_Sub2(struct inode *a_pstInode, struct file *a_pstFile);
extern int DW9718SAF_GetFileName_Sub2(unsigned char *pFileName);
//drv added ,wangmd, support DW9718SAF,20240705 end
#endif
