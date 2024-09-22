
#ifndef __GC9A01__H__
#define __GC9A01__H__


#ifndef CHAR_LF
#define CHAR_LF		0x0A
#endif

extern void gc9a01_bl_ctl(int on);

extern void gc9a01_reset_ctl(int on);

extern void gc9a01_dc_ctl(int on);

extern int gc9a01_scr_on(struct spi_device *spi);
extern int gc9a01_probe(struct spi_device *spi);
#endif


