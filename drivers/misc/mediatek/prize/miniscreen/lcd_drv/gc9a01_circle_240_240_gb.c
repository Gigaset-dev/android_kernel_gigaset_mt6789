
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/timer.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/spi/spi.h>
#include <linux/workqueue.h>
#include <linux/regulator/consumer.h>
#ifdef CONFIG_PM_SLEEP
#include <linux/pm_wakeup.h>
#endif

#define DPS_DEV_NAME  "mediatek,gc9a01"

int miniscreen_width = 0;
int miniscreen_height = 0;

struct gc9a01_struct {
	struct delayed_work dwork;
	struct spi_device *spi;
	struct regulator *vdd;
};

struct gc9a01_struct gc9a01_data;

static struct pinctrl *gc9a01_pinctrl;

static struct pinctrl_state *gc9a01_reset_active;
static struct pinctrl_state *gc9a01_reset_suspend;

static struct pinctrl_state *gc9a01_bl_active;
static struct pinctrl_state *gc9a01_bl_suspend;

static struct pinctrl_state *gc9a01_dc_active;
static struct pinctrl_state *gc9a01_dc_suspend;

static struct pinctrl_state *spi1_as_cs;
static struct pinctrl_state *spi1_as_ck;
static struct pinctrl_state *spi1_as_mi;
static struct pinctrl_state *spi1_as_mo;
static struct pinctrl_state *gc9a01_bl_pwm;

static int gc9a01_pinctrl_init(struct device *dev)
{
	int ret = 0;

	/* get pinctrl */
	gc9a01_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(gc9a01_pinctrl)) {
		pr_err("Failed to get devm_pinctrl_get\n");
		ret = PTR_ERR(gc9a01_pinctrl);
	}

	/* get reset pinctrl */
	gc9a01_reset_active = pinctrl_lookup_state(gc9a01_pinctrl, "reset_active");
	if (IS_ERR(gc9a01_reset_active)) {
		pr_err("Failed to init gc9a01_reset_active\n");
		ret = PTR_ERR(gc9a01_reset_active);
	}
	gc9a01_reset_suspend = pinctrl_lookup_state(gc9a01_pinctrl, "reset_suspend");
	if (IS_ERR(gc9a01_reset_suspend)) {
		pr_err("Failed to init gc9a01_reset_suspend\n");
		ret = PTR_ERR(gc9a01_reset_suspend);
	}

	/* get bl pinctrl */
	gc9a01_bl_active = pinctrl_lookup_state(gc9a01_pinctrl, "bl_active");
	if (IS_ERR(gc9a01_bl_active)) {
		pr_err("Failed to init gc9a01_bl_active\n");
		ret = PTR_ERR(gc9a01_bl_active);
	}
	gc9a01_bl_suspend = pinctrl_lookup_state(gc9a01_pinctrl, "bl_suspend");
	if (IS_ERR(gc9a01_bl_suspend)) {
		pr_err("Failed to init gc9a01_bl_suspend\n");
		ret = PTR_ERR(gc9a01_bl_suspend);
	}

	/* get dc pinctrl */
	gc9a01_dc_active = pinctrl_lookup_state(gc9a01_pinctrl, "dc_active");
	if (IS_ERR(gc9a01_dc_active)) {
		pr_err("Failed to init gc9a01_dc_active\n");
		ret = PTR_ERR(gc9a01_dc_active);
	}
	gc9a01_dc_suspend = pinctrl_lookup_state(gc9a01_pinctrl, "dc_suspend");
	if (IS_ERR(gc9a01_dc_suspend)) {
		pr_err("Failed to init gc9a01_dc_suspend\n");
		ret = PTR_ERR(gc9a01_dc_suspend);
	}

	/* get spi pinctrl */
	spi1_as_cs = pinctrl_lookup_state(gc9a01_pinctrl, "spi1_as_cs_t");
	if (IS_ERR(spi1_as_cs)) {
		pr_err("Failed to init spi1_as_cs\n");
		ret = PTR_ERR(spi1_as_cs);
	}
	spi1_as_ck = pinctrl_lookup_state(gc9a01_pinctrl, "spi1_as_ck_t");
	if (IS_ERR(spi1_as_ck)) {
		pr_err("Failed to init spi1_as_ck\n");
		ret = PTR_ERR(spi1_as_ck);
	}
	spi1_as_mi = pinctrl_lookup_state(gc9a01_pinctrl, "spi1_as_mi_t");
	if (IS_ERR(spi1_as_mi)) {
		pr_err("Failed to init spi1_as_mi\n");
		ret = PTR_ERR(spi1_as_mi);
	}
	spi1_as_mo = pinctrl_lookup_state(gc9a01_pinctrl, "spi1_as_mo_t");
	if (IS_ERR(spi1_as_mo)) {
		pr_err("Failed to init spi1_as_mo\n");
		ret = PTR_ERR(spi1_as_mo);
	}

	/* get bl pwm pinctrl */
	gc9a01_bl_pwm = pinctrl_lookup_state(gc9a01_pinctrl, "bl_pwm");
	if (IS_ERR(gc9a01_bl_pwm)) {
		pr_err("Failed to init gc9a01_bl_pwm\n");
		ret = PTR_ERR(gc9a01_bl_pwm);
	}

	pinctrl_select_state(gc9a01_pinctrl, spi1_as_cs);
	pinctrl_select_state(gc9a01_pinctrl, spi1_as_ck);
	pinctrl_select_state(gc9a01_pinctrl, spi1_as_mi);
	pinctrl_select_state(gc9a01_pinctrl, spi1_as_mo);
	pinctrl_select_state(gc9a01_pinctrl, gc9a01_bl_suspend);

	return ret;
}

void gc9a01_bl_ctl(int level)
{
	pr_err("%s %d level=%d\n", __func__, __LINE__, level);

	if (level)
		pinctrl_select_state(gc9a01_pinctrl, gc9a01_bl_active);
	else
		pinctrl_select_state(gc9a01_pinctrl, gc9a01_bl_suspend);
}

void gc9a01_reset_ctl(int on)
{
	pr_err("%s %d on=%d\n", __func__, __LINE__, on);

	if (on)
		pinctrl_select_state(gc9a01_pinctrl, gc9a01_reset_active);
	else
		pinctrl_select_state(gc9a01_pinctrl, gc9a01_reset_suspend);
}

void gc9a01_dc_ctl(int on)
{
	// pr_err("%s %d on=%d\n", __func__, __LINE__, on);

	if (on)
		pinctrl_select_state(gc9a01_pinctrl, gc9a01_dc_active);
	else
		pinctrl_select_state(gc9a01_pinctrl, gc9a01_dc_suspend);
}

static int gc9a01_sync_write(uint8_t *tx, uint32_t len)
{
	int ret = 0;
	struct spi_message m;
	struct spi_transfer t = {
		.tx_buf = tx,
		.len = len,
		//.speed_hz	= gc9a01_data.spi->speed_hz,
	};

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	ret = spi_sync(gc9a01_data.spi, &m);
	if (ret == 0)
		return m.actual_length;
	
	return ret;
}

int gc9a01_write_cmd(uint8_t cmd)
{
	uint8_t tx_buf[2] = {0};
	int ret = 0;
	
	gc9a01_dc_ctl(0);
	tx_buf[0] = cmd;
	ret = gc9a01_sync_write(tx_buf, 1);
	gc9a01_dc_ctl(1);
	
	return ret;
}

int gc9a01_write_data(uint8_t data)
{
	uint8_t tx_buf[2] = {0};
	int ret = 0;
	
	gc9a01_dc_ctl(1);
	tx_buf[0] = data;
	ret = gc9a01_sync_write(tx_buf, 1);
	gc9a01_dc_ctl(1);
	
	return ret;
}

void lcd_gc9a01_set_window(uint16_t xStart, uint16_t yStart, uint16_t xEnd, uint16_t yEnd, uint16_t xOffset, uint16_t yOffset)
{
	yStart = yStart + yOffset;
	yEnd = yEnd + yOffset;
	xStart = xStart + xOffset;
	xEnd = xEnd + xOffset;

	gc9a01_write_cmd(0x2A);
	gc9a01_write_data(xStart>>8);
	gc9a01_write_data(xStart&0xff);
	gc9a01_write_data(xEnd>>8);
	gc9a01_write_data(xEnd&0xff);
	
	gc9a01_write_cmd(0x2B);
	gc9a01_write_data(yStart>>8);
	gc9a01_write_data(yStart&0xff);
	gc9a01_write_data(yEnd>>8);
	gc9a01_write_data(yEnd&0xff);

	gc9a01_write_cmd(0x2c);
}

int gc9a01_scr_on(void)
{
	schedule_delayed_work(&gc9a01_data.dwork, 2*HZ);
	return 0;
}
EXPORT_SYMBOL(gc9a01_scr_on);

#define SPI_MAX_SPEED_HZ     (48000000)

#define MINISCREEN_WIDTH     (240)
#define MINISCREEN_HEIGHT    (240)

void gc9a01_init(void)
{
	pr_err("%s %d\n", __func__, __LINE__);
	gc9a01_data.spi->mode = 0;//SPI_CPOL;
	gc9a01_data.spi->bits_per_word = 8;
	gc9a01_data.spi->max_speed_hz = SPI_MAX_SPEED_HZ;
	spi_setup(gc9a01_data.spi);

	gc9a01_reset_ctl(1);
	msleep(50);
	gc9a01_reset_ctl(0);
	msleep(50);
	gc9a01_reset_ctl(1);
	msleep(120);

	gc9a01_write_cmd(0xFE);
	gc9a01_write_cmd(0xEF);

	gc9a01_write_cmd(0xEB);
	gc9a01_write_data(0x14);

	gc9a01_write_cmd(0x84);
	gc9a01_write_data(0x60); //40->60 0xb5 en  20210529  james

	gc9a01_write_cmd(0x88);
	gc9a01_write_data(0x0A);

	gc9a01_write_cmd(0x89);
	gc9a01_write_data(0x23);  ///gc9a01_data.spi 2data reg en  20210529

	gc9a01_write_cmd(0x8A);
	gc9a01_write_data(0x00);

	gc9a01_write_cmd(0x8B);
	gc9a01_write_data(0x80);

	gc9a01_write_cmd(0x8C);
	gc9a01_write_data(0x01);

	gc9a01_write_cmd(0x8D);
	gc9a01_write_data(0x03);

	gc9a01_write_cmd(0x8F);
	gc9a01_write_data(0xFF);
	gc9a01_write_cmd(0xB5);
	gc9a01_write_data(0x08);
	gc9a01_write_data(0x09);//  james add 20210529
	gc9a01_write_data(0x14);
	gc9a01_write_data(0x08);

	gc9a01_write_cmd(0xB6);
	gc9a01_write_data(0x00);
	gc9a01_write_data(0x60);

	gc9a01_write_cmd(0x36);
	gc9a01_write_data(0x48);

	gc9a01_write_cmd(0x3A);
	gc9a01_write_data(0x05);

	gc9a01_write_cmd(0x90);
	gc9a01_write_data(0x08);
	gc9a01_write_data(0x08);
	gc9a01_write_data(0x08);
	gc9a01_write_data(0x08); 

	gc9a01_write_cmd(0xBA);
	gc9a01_write_data(0x01);//TE WIDTH

	gc9a01_write_cmd(0xBD);
	gc9a01_write_data(0x06);

	gc9a01_write_cmd(0xBC);
	gc9a01_write_data(0x00);

	gc9a01_write_cmd(0xFF);
	gc9a01_write_data(0x60);
	gc9a01_write_data(0x01);
	gc9a01_write_data(0x04);

	gc9a01_write_cmd(0xC3);
	gc9a01_write_data(0x2F);
	gc9a01_write_cmd(0xC4);
	gc9a01_write_data(0x2F);

	gc9a01_write_cmd(0xC9);
	gc9a01_write_data(0x25);

	gc9a01_write_cmd(0xBE);
	gc9a01_write_data(0x11);

	gc9a01_write_cmd(0xE1);
	gc9a01_write_data(0x10);
	gc9a01_write_data(0x0E);

	gc9a01_write_cmd(0xDF);
	gc9a01_write_data(0x21);
	gc9a01_write_data(0x10);
	gc9a01_write_data(0x02);

	gc9a01_write_cmd(0xF0);
	gc9a01_write_data(0x49);
	gc9a01_write_data(0x0e);
	gc9a01_write_data(0x09);
	gc9a01_write_data(0x09);
	gc9a01_write_data(0x25);
	gc9a01_write_data(0x2e);

	gc9a01_write_cmd(0xF1);
	gc9a01_write_data(0x44);
	gc9a01_write_data(0x70);
	gc9a01_write_data(0x73);
	gc9a01_write_data(0x2F);
	gc9a01_write_data(0x30);
	gc9a01_write_data(0x6F);

	gc9a01_write_cmd(0xF2);
	gc9a01_write_data(0x49);
	gc9a01_write_data(0x0e);
	gc9a01_write_data(0x09);
	gc9a01_write_data(0x09);
	gc9a01_write_data(0x25);
	gc9a01_write_data(0x2e);

	gc9a01_write_cmd(0xF3);
	gc9a01_write_data(0x44);
	gc9a01_write_data(0x70);
	gc9a01_write_data(0x73);
	gc9a01_write_data(0x2F);
	gc9a01_write_data(0x30);
	gc9a01_write_data(0x6F);

	gc9a01_write_cmd(0xED);	
	gc9a01_write_data(0x1B); 
	gc9a01_write_data(0x8B); 

	gc9a01_write_cmd(0xAE);
	gc9a01_write_data(0x77);

	gc9a01_write_cmd(0xCD);
	gc9a01_write_data(0x63);

	gc9a01_write_cmd(0xAC);
	gc9a01_write_data(0x27);

	gc9a01_write_cmd(0x70);
	gc9a01_write_data(0x07);
	gc9a01_write_data(0x07);
	gc9a01_write_data(0x04);
	gc9a01_write_data(0x06);//VGH
	gc9a01_write_data(0x0F); //VGL
	gc9a01_write_data(0x09);
	gc9a01_write_data(0x07);
	gc9a01_write_data(0x08);
	gc9a01_write_data(0x03);

	gc9a01_write_cmd(0xE8);
	gc9a01_write_data(0x24);

	gc9a01_write_cmd(0x60);
	gc9a01_write_data(0x38);
	gc9a01_write_data(0x0B);
	gc9a01_write_data(0x6D);
	gc9a01_write_data(0x6D);

	gc9a01_write_data(0x39);
	gc9a01_write_data(0xF0);
	gc9a01_write_data(0x6D);
	gc9a01_write_data(0x6D);

	gc9a01_write_cmd(0x61);
	gc9a01_write_data(0x38);
	gc9a01_write_data(0xF4);
	gc9a01_write_data(0x6D);
	gc9a01_write_data(0x6D);

	gc9a01_write_data(0x38);
	gc9a01_write_data(0xF7);
	gc9a01_write_data(0x6D);
	gc9a01_write_data(0x6D);
	/////////////////////////////////////
	gc9a01_write_cmd(0x62);
	gc9a01_write_data(0x38);
	gc9a01_write_data(0x0D);
	gc9a01_write_data(0x71);
	gc9a01_write_data(0xED);
	gc9a01_write_data(0x70);
	gc9a01_write_data(0x70);
	gc9a01_write_data(0x38);
	gc9a01_write_data(0x0F);
	gc9a01_write_data(0x71);
	gc9a01_write_data(0xEF);
	gc9a01_write_data(0x70);
	gc9a01_write_data(0x70);

	gc9a01_write_cmd(0x63);
	gc9a01_write_data(0x38);
	gc9a01_write_data(0x11);
	gc9a01_write_data(0x71);
	gc9a01_write_data(0xF1);
	gc9a01_write_data(0x70);
	gc9a01_write_data(0x70);
	gc9a01_write_data(0x38);
	gc9a01_write_data(0x13);
	gc9a01_write_data(0x71);
	gc9a01_write_data(0xF3);
	gc9a01_write_data(0x70);
	gc9a01_write_data(0x70);

	gc9a01_write_cmd(0x64);
	gc9a01_write_data(0x28);
	gc9a01_write_data(0x29);
	gc9a01_write_data(0xF1);
	gc9a01_write_data(0x01);
	gc9a01_write_data(0xF1);
	gc9a01_write_data(0x00);//
	gc9a01_write_data(0x1a);//

	gc9a01_write_cmd(0x66);
	gc9a01_write_data(0x3C);
	gc9a01_write_data(0x00);
	gc9a01_write_data(0x98);
	gc9a01_write_data(0x10);
	gc9a01_write_data(0x32);
	gc9a01_write_data(0x45);
	gc9a01_write_data(0x01);
	gc9a01_write_data(0x00);
	gc9a01_write_data(0x00);
	gc9a01_write_data(0x00);
	gc9a01_write_cmd(0x67);
	gc9a01_write_data(0x00);
	gc9a01_write_data(0x3C);
	gc9a01_write_data(0x00);
	gc9a01_write_data(0x00);
	gc9a01_write_data(0x00);
	gc9a01_write_data(0x10);
	gc9a01_write_data(0x54);
	gc9a01_write_data(0x67);
	gc9a01_write_data(0x45);
	gc9a01_write_data(0xcd);

	gc9a01_write_cmd(0x74);
	gc9a01_write_data(0x10);
	gc9a01_write_data(0x85);	//85
	gc9a01_write_data(0x80);
	gc9a01_write_data(0x00);
	gc9a01_write_data(0x00);
	gc9a01_write_data(0x4E);
	gc9a01_write_data(0x00);					

	gc9a01_write_cmd(0x98);
	gc9a01_write_data(0x3e);
	gc9a01_write_data(0x07);
	gc9a01_write_cmd(0x99);
	gc9a01_write_data(0x3e);
	gc9a01_write_data(0x07);

	gc9a01_write_cmd(0x35);
	gc9a01_write_cmd(0x21);
	msleep(10);
	gc9a01_write_cmd(0x11);
	msleep(120);
	gc9a01_write_cmd(0x29);
	msleep(20);
	gc9a01_write_cmd(0x2C);

	lcd_gc9a01_set_window(0, 0, MINISCREEN_WIDTH - 1, MINISCREEN_HEIGHT - 1, 0, 0);
}

static void gc9a01_fill(void)
{
	uint16_t r[MINISCREEN_WIDTH] = {0};
	uint16_t g[MINISCREEN_WIDTH] = {0};
	uint16_t b[MINISCREEN_WIDTH] = {0};
	//int ret = 0;
	int height = MINISCREEN_HEIGHT;
	int strip_w = 30;
	int i = 0;

	for(i=0;i<MINISCREEN_WIDTH;i++)
		*(r+i) = 0x00f8;
	for(i=0;i<MINISCREEN_WIDTH;i++)
		*(g+i) = 0xe007;
	for(i=0;i<MINISCREEN_WIDTH;i++)
		*(b+i) = 0x1f00;

#if 0
	printk("%s %x %x %x %x %d\n",__func__, *((uint8_t *)r),*((uint8_t *)r+1), *((uint8_t *)r+2),*((uint8_t *)r+3), sizeof(r));
	for(i=0;i<MINISCREEN_HEIGHT;i++){
		ret = gc9a01_sync_write((uint8_t *)r, sizeof(r));
		printk("%s %d %d\n",__func__,i, ret);
	}
#endif
	do {
		if (height < strip_w) {
			strip_w = height;
			if (height <= 0)
				break;
		}
		for (i = 0; i < strip_w; i++) {
			gc9a01_sync_write((uint8_t *)r, sizeof(r));
			height -= 1;
		}
		if (height < strip_w){
			strip_w = height;
			if (height <= 0)
				break;
		}
		for (i = 0; i < strip_w; i++) {
			gc9a01_sync_write((uint8_t *)g, sizeof(r));
			height -= 1;
		}
		if (height < strip_w){
			strip_w = height;
			if (height <= 0)
				break;
		}
		for (i = 0; i < strip_w; i++) {
			gc9a01_sync_write((uint8_t *)b, sizeof(r));
			height -= 1;
		}
	} while(height > 0);
}

static void gc9a01_dwork(struct work_struct *work)
{
	gc9a01_init();
	gc9a01_fill();
	gc9a01_bl_ctl(85);
}

int gc9a01_probe(struct spi_device *spi)
{
	int err = 0;

	pr_err("%s %d\n", __func__, __LINE__);

	miniscreen_width = MINISCREEN_WIDTH;
	miniscreen_height = MINISCREEN_HEIGHT;

	/* init pinctrl */
	if (gc9a01_pinctrl_init(&spi->dev)) {
		pr_err("gc9a01_pinctrl_init err\n");
		err = -EFAULT;
		goto error;
	} else {
		pr_err("gc9a01_pinctrl_init success\n");
	}

	gc9a01_reset_ctl(0);

	gc9a01_data.spi = spi;
	INIT_DELAYED_WORK(&gc9a01_data.dwork, gc9a01_dwork);

	return 0;
error:
	return err;
}

EXPORT_SYMBOL(miniscreen_width);
EXPORT_SYMBOL(miniscreen_height);
EXPORT_SYMBOL(gc9a01_init);
EXPORT_SYMBOL(gc9a01_bl_ctl);
EXPORT_SYMBOL(gc9a01_reset_ctl);
EXPORT_SYMBOL(gc9a01_dc_ctl);
EXPORT_SYMBOL(gc9a01_probe);