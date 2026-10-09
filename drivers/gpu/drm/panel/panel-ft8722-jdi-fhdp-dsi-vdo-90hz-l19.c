/*
 * Copyright (c) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <drm/drmP.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <linux/backlight.h>
#include <linux/delay.h>

#include <linux/gpio/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/hqsysfs.h>
/*L19 code for HQ-159420 by chenzimo at 2021/12/20 start*/
#include <drm/drm_notifier_odm.h>
/*L19 code for HQ-159420 by chenzimo at 2021/12/20 end*/
/*L19 code for HQ-172682 by caogaojie at 2021/12/23 start*/
#include <linux/gfp.h>
#include <linux/kmemleak.h>
/*L19 code for HQ-172682 by caogaojie at 2021/12/23 end*/
#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mtk_drm_graphics_base.h"
#include "../mediatek/mtk_log.h"
#include "../mediatek/mtk_panel_ext.h"
#endif
/* enable this to check panel self -bist pattern */
/* #define PANEL_BIST_PATTERN */
/****************TPS65132***********/
#include <linux/i2c-dev.h>
#include <linux/i2c.h>

/*L19 code for HQ-159316 by chenzimo at 20211119 start*/
#define PHYSICAL_WIDTH 68429
#define PHYSICAL_HEIGHT 152571
/*L19 code for HQ-159316 by chenzimo at 20211119 end*/

extern bool fts_gesture_flag;

extern int32_t fts_ts_tp_suspend(void);
extern int32_t fts_ts_tp_resume(void);
extern bool esd_flag;
void fts_fwresume_work(void);

extern bool tp_ready;

static struct regulator *lcm_1v3 = NULL;
/*L19 code for HQ-171436 by caogaojie at 2021/12/8 start*/
static const char *panel_name = "panel_name=dsi_panel_l19_31_03_0b_dsc_vdo_lcm_drv";
extern int ktd_hbm_set(enum backlight_hbm_mode hbm_mode);
extern int ti_hbm_set(enum backlight_hbm_mode hbm_mode);
/*L19 code for HQ-171436 by caogaojie at 2021/12/8 end*/
/*L19 code for HQ-172682 by caogaojie at 2021/12/23 start*/
extern int real_refresh;
extern int temp_refresh;
/*L19 code for HQ-172682 by caogaojie at 2021/12/23 end*/
#define HFP_SUPPORT 0
#if HFP_SUPPORT
static int current_fps = 90;
#endif
extern int lcm_name[10];
static char bl_tb0[] = { 0x51, 0xff };
extern int _lcm_i2c_write_bytes(unsigned char addr, unsigned char value);
/*L19 code for HQ-159420 by chenzimo at 2021/12/20 start*/
struct drm_notifier_data g_notify_data1;
/*L19 code for HQ-159420 by chenzimo at 2021/12/20 end*/

struct jdi {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *bias_pos;
	struct gpio_desc *bias_neg;
	/*L19 code for HQ-159431 by caogaojie at 2021/12/5 start*/
	struct gpio_desc *pwm;
	/*L19 code for HQ-159431 by caogaojie at 2021/12/5 end*/
	bool prepared;
	bool enabled;

	int error;
	/*L19 code for HQ-171436 by caogaojie at 2021/12/8 start*/
	const char *panel_info;
	/*L19 code for HQ-171436 by caogaojie at 2021/12/8 end*/
};

#define jdi_dcs_write_seq(ctx, seq...)                                         \
	({                                                                     \
		const u8 d[] = { seq };                                        \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		jdi_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

#define jdi_dcs_write_seq_static(ctx, seq...)                                  \
	({                                                                     \
		static const u8 d[] = { seq };                                 \
		jdi_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})
/*L19 code for HQ-172682 by caogaojie at 2021/12/23 start*/
static inline struct jdi *panel_to_jdi(struct drm_panel *panel)
{
	return container_of(panel, struct jdi, panel);
}

static ssize_t mtkfb_get_refresh(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret;
	ret = scnprintf(buf, PAGE_SIZE, "%3d\n", real_refresh);
	return ret;
}
static ssize_t mtkfb_set_refresh(struct device *dev, struct device_attribute *attr, const char *buf, size_t len)
{
		sscanf(buf, "%3d", &real_refresh);
		return len;
}
static DEVICE_ATTR(mtkfb_fps, 0644, mtkfb_get_refresh, mtkfb_set_refresh);
/*L19 code for HQ-172682 by caogaojie at 2021/12/23 end*/
#ifdef PANEL_SUPPORT_READBACK
static int jdi_dcs_read(struct jdi *ctx, u8 cmd, void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;

	if (ctx->error < 0)
		return 0;

	ret = mipi_dsi_dcs_read(dsi, cmd, data, len);
	if (ret < 0) {
		dev_info(ctx->dev, "error %d reading dcs seq:(%#x)\n", ret,
			 cmd);
		ctx->error = ret;
	}

	return ret;
}

static void jdi_panel_get_data(struct jdi *ctx)
{
	u8 buffer[3] = { 0 };
	static int ret;

	pr_info("%s+\n", __func__);

	if (ret == 0) {
		ret = jdi_dcs_read(ctx, 0x0A, buffer, 1);
		pr_info("%s  0x%08x\n", __func__, buffer[0] | (buffer[1] << 8));
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

static void jdi_dcs_write(struct jdi *ctx, const void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;
	char *addr;

	if (ctx->error < 0)
		return;

	addr = (char *)data;
	if ((int)*addr < 0xB0)
		ret = mipi_dsi_dcs_write_buffer(dsi, data, len);
	else
		ret = mipi_dsi_generic_write(dsi, data, len);
	if (ret < 0) {
		dev_info(ctx->dev, "error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
	}
}

/*L19 code for HQ-171436 by caogaojie at 2021/12/8 start*/
static int jdi_panel_get_panel_info(struct drm_panel *panel, char *buf)
{
  	int count = 0;
  	struct jdi *ctx = panel_to_jdi(panel);

	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}

	ctx = panel_to_jdi(panel);
	count = snprintf(buf, PAGE_SIZE, "%s\n", ctx->panel_info);
 	return count;
}

static int jdi_panel_hbm_control(struct drm_panel *panel,int mode)
{
	extern char *saved_command_line;
	unsigned int hbm_mode;
	int bkl_id = 0;
	char *bkl_ptr = (char *)strnstr(saved_command_line, ":bklic=", strlen(saved_command_line));
	bkl_ptr += strlen(":bklic=");
	bkl_id = simple_strtol(bkl_ptr, NULL, 10);
	//sscanf(buf, "%d", &hbm_mode);
	hbm_mode = mode;
 	if (hbm_mode >= HBM_MODE_LEVEL_MAX)
		hbm_mode = HBM_MODE_LEVEL_MAX - 1;
	if (hbm_mode < HBM_MODE_DEFAULT)
 		hbm_mode = HBM_MODE_DEFAULT;
	/*L19 code for L19-142 by chenzimo at 2022/3/8 start*/
	if (bkl_id == 1) {
		ti_hbm_set((enum backlight_hbm_mode)hbm_mode);
		printk("[%s]: Ti, set hbm_mode = %d\n", __func__, hbm_mode);
	} else if (bkl_id == 24) {
		ktd_hbm_set((enum backlight_hbm_mode)hbm_mode);
		printk("[%s]: ktd, set hbm_mode = %d\n", __func__, hbm_mode);
	} else {
		printk("[%s]: No backlight\n", __func__);
	}
	/*L19 code for L19-142 by chenzimo at 2022/3/8 end*/
	pr_info("%s-\n", __func__);
	return 0;
}
/*L19 code for HQ-171436 by caogaojie at 2021/12/8 end*/

static void jdi_panel_init(struct jdi *ctx)
{
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	/*L19 code for HQ-159431 by caogaojie at 2021/12/5 start*/
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(5000, 5001);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(2000, 2001);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(12000, 12001);
	/*L19 code for HQ-159431 by caogaojie at 2021/12/5 end*/
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	pr_info("%s+\n", __func__);

	/*L19 code for L19-81 by caogaojie at 2021/12/31 start*/
	jdi_dcs_write_seq_static(ctx,0x1c,0x02);
	jdi_dcs_write_seq_static(ctx,0x35,0X00);
	/*L19 code for L19-81 by caogaojie at 2021/12/31 end*/

	//SLPOUT and DISPON
	jdi_dcs_write_seq_static(ctx,0x11);
	msleep(120);
	jdi_dcs_write_seq_static(ctx,0x29);
	msleep(20);

		pr_info("%s-\n", __func__);
}

static int jdi_disable(struct drm_panel *panel)
{
	struct jdi *ctx = panel_to_jdi(panel);

	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	return 0;
}

static int jdi_unprepare(struct drm_panel *panel)
{
	/*L19 code for HQ-159420 by chenzimo at 2021/12/20 start*/
	int blank;
	/*L19 code for HQ-159420 by chenzimo at 2021/12/20 end*/
	struct jdi *ctx = panel_to_jdi(panel);

	pr_info("%s++\n", __func__);

	if (!ctx->prepared)
		return 0;

	if (esd_flag == true) {
	    pr_info("%s, Now esd_flag = %d\n", __func__, esd_flag);
	    fts_ts_tp_suspend();
	}
	/*L19 code for HQ-159420 by chenzimo at 2021/12/20 start*/
	blank = DRM_BLANK_POWERDOWN;
	g_notify_data1.data = &blank;
	drm_notifier_call_chain(DRM_EVENT_BLANK, &g_notify_data1);
	pr_err("[XMFP] : %s ++++ blank = DRM_BLANK_POWERDOWN ++++", __func__);
	/*L19 code for HQ-159420 by chenzimo at 2021/12/20 end*/

	/*L19 code for HQ-159441 by sunfeiting at 2021/11/24 start*/
	jdi_dcs_write_seq_static(ctx, MIPI_DCS_SET_DISPLAY_OFF);
	msleep(20);
	jdi_dcs_write_seq_static(ctx, MIPI_DCS_ENTER_SLEEP_MODE);
        jdi_dcs_write_seq_static(ctx, 0x35, 0x00);
	msleep(120);
	jdi_dcs_write_seq_static(ctx, 0x00, 0x00);
	jdi_dcs_write_seq_static(ctx, 0xF7, 0x5A, 0xA5, 0x95, 0x27);
	/*L19 code for HQ-159431 by caogaojie at 2021/12/5 start*/
	usleep_range(3000, 3001);
	/*L19 code for HQ-159431 by caogaojie at 2021/12/5 end*/
	/*L19 code for HQ-159441 by sunfeiting at 2021/11/24 end*/
	/*
	 * ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	 * gpiod_set_value(ctx->reset_gpio, 0);
	 * devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	 */
	
	if (fts_gesture_flag == true) {
		ctx->bias_neg =
			devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->bias_neg, 1);
		devm_gpiod_put(ctx->dev, ctx->bias_neg);
		usleep_range(2000, 2001);
		ctx->bias_pos =
		devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->bias_pos, 1);
		devm_gpiod_put(ctx->dev, ctx->bias_pos);
	} else {
		ctx->bias_neg =
			devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->bias_neg, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_neg);
		usleep_range(2000, 2001);
		ctx->bias_pos =
			devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->bias_pos, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_pos);
	}
	
	devm_gpiod_put(ctx->dev, ctx->bias_pos);
	/*L19 code for L19-120 by feiwen at 2022/01/26 start*/
	usleep_range(6000, 6001);
	regulator_disable(lcm_1v3);
	/*L19 code for L19-120 by feiwen at 2022/01/26 end*/

	ctx->error = 0;
	ctx->prepared = false;

	return 0;
}

static int jdi_prepare(struct drm_panel *panel)
{
	struct jdi *ctx = panel_to_jdi(panel);
	/*L19 code for HQ-159420 by chenzimo at 2021/12/20 start*/
	int blank;
	/*L19 code for HQ-159420 by chenzimo at 2021/12/20 end*/
	int ret;

	pr_info("%s+\n", __func__);
	if (ctx->prepared)
		return 0;

	// lcd reset H -> L -> L
	/*L19 code for HQ-159431 by caogaojie at 2021/12/5 start*/
	usleep_range(2000, 2001);
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(3000, 3001);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(1000, 1001);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(2000, 2001);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	/*L19 code for HQ-159431 by caogaojie at 2021/12/5 end*/
	// end
	regulator_set_voltage(lcm_1v3, 1300000, 1300000);
	regulator_enable(lcm_1v3);
	msleep(1);
	if( tp_ready == true )
{
	fts_fwresume_work();
}
	ctx->bias_pos =
	    devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->bias_pos, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_pos);

	/*L19 code for L19-120 by feiwen at 2022/01/26 start*/
	usleep_range(4000, 4001);
	/*L19 code for L19-120 by feiwen at 2022/01/26 end*/
	ctx->bias_neg =
	    devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->bias_neg, 1);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);
	_lcm_i2c_write_bytes(0x0, 0xf);
	_lcm_i2c_write_bytes(0x1, 0xf);
	/*L19 code for HQ-159431 by caogaojie at 2021/12/5 start*/
	usleep_range(5000, 5001);

	jdi_panel_init(ctx);

	ctx->pwm =
	    devm_gpiod_get(ctx->dev, "pwm", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->pwm, 1);
	devm_gpiod_put(ctx->dev, ctx->pwm);
	/*L19 code for HQ-159431 by caogaojie at 2021/12/5 end*/
	ret = ctx->error;
	if (ret < 0)
		jdi_unprepare(panel);

	ctx->prepared = true;
#ifdef PANEL_SUPPORT_READBACK
	jdi_panel_get_data(ctx);
#endif

#ifdef VENDOR_EDIT
	// shifan@bsp.tp 20191226 add for loading tp fw when screen lighting on
	//lcd_queue_load_tp_fw();
#endif

	if (esd_flag == true) {
	    pr_info("%s, Now esd_flag = %d\n", __func__, esd_flag);
	    fts_ts_tp_resume();
	}
	/*L19 code for HQ-159420 by chenzimo at 2021/12/20 start*/
	blank = DRM_BLANK_UNBLANK;
	g_notify_data1.data = &blank;
	drm_notifier_call_chain(DRM_EVENT_BLANK, &g_notify_data1);
	pr_err("[XMFP] : %s ++++ blank = DRM_BLANK_UNBLANK ++++", __func__);
	/*L19 code for HQ-159420 by chenzimo at 2021/12/20 end*/
	pr_info("%s-\n", __func__);
	return ret;
}

static int jdi_enable(struct drm_panel *panel)
{
	struct jdi *ctx = panel_to_jdi(panel);

	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;

	return 0;
}
#if HFP_SUPPORT
#define HFP_60HZ (944)
#define HFP_90HZ (256)
#define HSA (20)
#define HBP (22)
#define VFP (50)
#define VFP_45HZ (870)
#define VFP_60HZ (1280)
#define VSA (10)
#define VBP (4)
#define VAC (2400)
#define HAC (1080)
static const struct drm_display_mode default_mode = {
	.clock = 300955,
	.hdisplay = HAC,
	.hsync_start = HAC + HFP_60HZ,
	.hsync_end = HAC + HFP_60HZ + HSA,
	.htotal = HAC + HFP_60HZ + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + VFP,
	.vsync_end = VAC + VFP + VSA,
	.vtotal = VAC + VFP + VSA + VBP,
	.vrefresh = 60,
	/*L19 code for HQ-159006 by chenzimo at 20211112 start*/
	.width_mm = PHYSICAL_WIDTH / 1000,
	.height_mm = PHYSICAL_HEIGHT / 1000,
	/*L19 code for HQ-159006 by chenzimo at 20211112 end*/
};

static const struct drm_display_mode performance_mode = {
	.clock = 301036,
	.hdisplay = HAC,
	.hsync_start = HAC + HFP_90HZ,
	.hsync_end = HAC + HFP_90HZ + HSA,
	.htotal = HAC + HFP_90HZ + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + VFP,
	.vsync_end = VAC + VFP + VSA,
	.vtotal = VAC + VFP + VSA + VBP,
	.vrefresh = 90,
	/*L19 code for HQ-159006 by chenzimo at 20211112 start*/
	.width_mm = PHYSICAL_WIDTH / 1000,
	.height_mm = PHYSICAL_HEIGHT / 1000,
	/*L19 code for HQ-159006 by chenzimo at 20211112 end*/
};
#else
/*L19 code for L19-98 by caogaojie at 2022/1/11 start*/
#define HFP (268)
#define HSA (20)
#define HBP (24)
#define VFP_30HZ (5000)
#define VFP_60HZ (1290)
#define VFP_90HZ (46)
#define VSA (2)
#define VBP (18)
#define VAC (2408)
#define HAC (1080)
/*L19 code for L19-98 by caogaojie at 2021/1/11 end*/
static const struct drm_display_mode default_mode = {
	.clock = 301604,
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + VFP_60HZ,
	.vsync_end = VAC + VFP_60HZ + VSA,
	.vtotal = VAC + VFP_60HZ + VSA + VBP,
	.vrefresh = 60,
	/*L19 code for HQ-159006 by chenzimo at 20211112 start*/
	.width_mm = PHYSICAL_WIDTH / 1000,
	.height_mm = PHYSICAL_HEIGHT / 1000,
	/*L19 code for HQ-159006 by chenzimo at 20211112 end*/
};

static const struct drm_display_mode performance_mode = {
	.clock = 305586,
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + VFP_90HZ,
	.vsync_end = VAC + VFP_90HZ + VSA,
	.vtotal = VAC + VFP_90HZ + VSA + VBP,
	.vrefresh = 90,
	/*L19 code for HQ-159006 by chenzimo at 20211112 start*/
	.width_mm = PHYSICAL_WIDTH / 1000,
	.height_mm = PHYSICAL_HEIGHT / 1000,
	/*L19 code for HQ-159006 by chenzimo at 20211112 end*/
};
/*L19 code for HQ-173138 by caogaojie at 2021/12/21 start*/
static const struct drm_display_mode refresh_30_mode = {
	.clock = 302902,
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + VFP_30HZ,
	.vsync_end = VAC + VFP_30HZ + VSA,
	.vtotal = VAC + VFP_30HZ + VSA + VBP,
	.vrefresh = 30,
	.width_mm = PHYSICAL_WIDTH / 1000,
	.height_mm = PHYSICAL_HEIGHT / 1000,
};
/*L19 code for HQ-173138 by caogaojie at 2021/12/21 end*/
#endif

#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params = {
	/*L19 code for L19-98 by caogaojie at 2021/1/11 start*/
	.pll_clk = 545,
	/*L19 code for HQ-173138 by caogaojie at 2021/12/21 start*/
	.vfp_low_power = 2040,
	/*L19 code for HQ-173138 by caogaojie at 2021/12/21 end*/
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	/*L19 code for HQ-175407 by caogaojie at 2022/1/6 start*/
	.lcm_index = 1,
	/*L19 code for HQ-175407 by caogaojie at 2022/1/6 end*/
	.lcm_esd_check_table[0] = {
		.cmd = 0, .count = 1, .para_list[0] = 0x9C,
	},
	.is_cphy = 1,
	.data_rate = 1090,
	/*L19 code for L19-98 by caogaojie at 2021/1/11 end*/
	.dyn_fps = {
		.switch_en = 1,
#if HFP_SUPPORT
		.dfps_cmd_table[0] = {0, 2, {0xFF, 0x25} },
		.dfps_cmd_table[1] = {0, 2, {0xFB, 0x01} },
		.dfps_cmd_table[2] = {0, 2, {0x18, 0x21} },
		/*switch page for esd check*/
		.dfps_cmd_table[3] = {0, 2, {0xFF, 0x10} },
		.dfps_cmd_table[4] = {0, 2, {0xFB, 0x01} },
#else
		.vact_timing_fps = 90,
#endif
	},
	/*L19 code for HQ-159430 by chenzimo at 20211208 start*/
	.phy_timcon = {
		.hs_prpr = 9,
		.lpx = 12,
	},
	/*L19 code for HQ-159430 by chenzimo at 20211208 end*/
	/*L19 code for HQ-159316 by chenzimo at 20211119 start*/
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	/*L19 code for HQ-159316 by chenzimo at 20211119 end*/
	/*L19 code for L19-54 by chenzimo at 20211213 start*/
	.dyn = {
		.switch_en = 1,
		.pll_clk = 536,
		.data_rate = 1072,
	}
	/*L19 code for L19-54 by chenzimo at 20211213 end*/
};

static struct mtk_panel_params ext_params_90hz = {
	/*L19 code for L19-98 by caogaojie at 2021/1/11 start*/
	.pll_clk = 545,
	/*L19 code for HQ-173138 by caogaojie at 2021/12/21 start*/
	.vfp_low_power = 2040,
	/*L19 code for HQ-173138 by caogaojie at 2021/12/21 end*/
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	/*L19 code for HQ-175407 by caogaojie at 2022/1/6 start*/
	.lcm_index = 1,
	/*L19 code for HQ-175407 by caogaojie at 2022/1/6 end*/
	.lcm_esd_check_table[0] = {

		.cmd = 0, .count = 1, .para_list[0] = 0x9C,
	},
	.is_cphy = 1,
	.data_rate = 1090,
	/*L19 code for L19-98 by caogaojie at 2021/1/11 end*/
	.dyn_fps = {
		.switch_en = 1,
#if HFP_SUPPORT
		.dfps_cmd_table[0] = {0, 2, {0xFF, 0x25} },
		.dfps_cmd_table[1] = {0, 2, {0xFB, 0x01} },
		.dfps_cmd_table[2] = {0, 2, {0x18, 0x20} },
		/*switch page for esd check*/
		.dfps_cmd_table[3] = {0, 2, {0xFF, 0x10} },
		.dfps_cmd_table[4] = {0, 2, {0xFB, 0x01} },
#else
		.vact_timing_fps = 90,
#endif
	},
	/*L19 code for HQ-159430 by chenzimo at 20211208 start*/
	.phy_timcon = {
		.hs_prpr = 9,
		.lpx = 12,
	},
	/*L19 code for HQ-159430 by chenzimo at 20211208 end*/
	/*L19 code for HQ-159316 by chenzimo at 20211119 start*/
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	/*L19 code for HQ-159316 by chenzimo at 20211119 end*/
	/*L19 code for L19-54 by chenzimo at 20211213 start*/
	.dyn = {
		.switch_en = 1,
		.pll_clk = 536,
		.data_rate = 1072,
	}
	/*L19 code for L19-54 by chenzimo at 20211213 end*/
};

/*L19 code for HQ-173138 by caogaojie at 2021/12/21 start*/
static struct mtk_panel_params ext_params_30hz = {
/*L19 code for L19-98 by caogaojie at 2021/1/11 start*/
	.pll_clk = 545,
/*L19 code for HQ-172270 by caogaojie at 2021/12/23 start*/
	.vfp_low_power = 0,
/*L19 code for HQ-172270 by caogaojie at 2021/12/23 end*/
	.cust_esd_check = 0,
	.esd_check_enable = 1,
/*L19 code for HQ-175407 by caogaojie at 2022/1/6 start*/
	.lcm_index = 1,
/*L19 code for HQ-175407 by caogaojie at 2022/1/6 end*/
	.lcm_esd_check_table[0] = {
		.cmd = 0, .count = 1, .para_list[0] = 0x9C,
	},
	.is_cphy = 1,
	.data_rate = 1090,
/*L19 code for L19-98 by caogaojie at 2021/1/11 end*/
	.dyn_fps = {
		.switch_en = 1,
#if HFP_SUPPORT
		.dfps_cmd_table[0] = {0, 2, {0xFF, 0x25} },
		.dfps_cmd_table[1] = {0, 2, {0xFB, 0x01} },
		.dfps_cmd_table[2] = {0, 2, {0x18, 0x21} },
		/*switch page for esd check*/
		.dfps_cmd_table[3] = {0, 2, {0xFF, 0x10} },
		.dfps_cmd_table[4] = {0, 2, {0xFB, 0x01} },
#else
		.vact_timing_fps = 90,
#endif
	},
	.phy_timcon = {
		.hs_prpr = 9,
		.lpx = 12,
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.dyn = {
		.switch_en = 1,
		.pll_clk = 536,
		.data_rate = 1072,
	}
};
/*L19 code for HQ-173138 by caogaojie at 2021/12/21 end*/

static int jdi_panel_ata_check(struct drm_panel *panel)
{
	/* Customer test by own ATA tool */
	return 1;
}

static int jdi_setbacklight_cmdq(void *dsi, dcs_write_gce cb, void *handle,
				 unsigned int level)
{

	if (level > 255)
		level = 255;
	pr_info("%s backlight = -%d\n", __func__, level);
	bl_tb0[1] = (u8)level;
#if 0
	char bl_tb0[] = {0x51, 0xf, 0xff};

	if (level > 255)
		level = 255;

	level = level * 4095 / 255;
	bl_tb0[1] = ((level >> 8) & 0xf);
	bl_tb0[2] = (level & 0xff);
#endif
	if (!cb)
		return -1;

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));
	return 0;
}

struct drm_display_mode *jdi_get_mode_by_id_hfp(struct drm_panel *panel,
	unsigned int mode)
{
	struct drm_display_mode *m;
	unsigned int i = 0;

	list_for_each_entry(m, &panel->connector->modes, head) {
		if (i == mode)
			return m;
		i++;
	}
	return NULL;
}

/*L19 code for HQ-173138 by caogaojie at 2021/12/21 start*/
static int jdi_mtk_panel_ext_param_set(struct drm_panel *panel, unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;
	struct drm_display_mode *m = jdi_get_mode_by_id_hfp(panel, mode);

	if (m->vrefresh == 60) {
		ext->params = &ext_params;
#if HFP_SUPPORT
		current_fps = 60;
#endif
	} else if (m->vrefresh == 90) {
		ext->params = &ext_params_90hz;
#if HFP_SUPPORT
		current_fps = 90;
#endif
	} else if (m->vrefresh == 30) {
		ext->params = &ext_params_30hz;
#if HFP_SUPPORT
		current_fps = 30;
#endif 
	} else
		ret = 1;
/*L19 code for HQ-172682 by caogaojie at 2021/12/23 start*/
	temp_refresh = m->vrefresh;
	real_refresh = temp_refresh;
/*L19 code for HQ-172682 by caogaojie at 2021/12/23 end*/
	return ret;
}

static int jdi_mtk_panel_ext_param_get(struct mtk_panel_params *ext_para,
			 unsigned int mode)
{
	int ret = 0;

	if (mode == 0)
		ext_para = &ext_params;
	else if (mode == 1)
		ext_para = &ext_params_90hz;
	else if (mode == 2)
		ext_para = &ext_params_30hz;
	else
		ret = 1;

	return ret;

}
/*L19 code for HQ-173138 by caogaojie at 2021/12/21 end*/

static void jdi_mode_switch_to_90(struct drm_panel *panel)
{
	struct jdi *ctx = panel_to_jdi(panel);

	jdi_dcs_write_seq_static(ctx, 0xFF, 0x25);
	jdi_dcs_write_seq_static(ctx, 0xFB, 0x01);
	jdi_dcs_write_seq_static(ctx, 0x18, 0x20);//90hz

}

static void jdi_mode_switch_to_60(struct drm_panel *panel)
{
	struct jdi *ctx = panel_to_jdi(panel);

	jdi_dcs_write_seq_static(ctx, 0xFF, 0x25);
	jdi_dcs_write_seq_static(ctx, 0xFB, 0x01);
	jdi_dcs_write_seq_static(ctx, 0x18, 0x21);
}

static int jdi_mode_switch(struct drm_panel *panel, unsigned int cur_mode,
		unsigned int dst_mode, enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	int ret = 0;
	//struct drm_display_mode *m = get_mode_by_id(panel, dst_mode);

	pr_info("%s cur_mode = %d dst_mode %d\n", __func__, cur_mode, dst_mode);

	if (dst_mode == 60) { /* 60 switch to 120 */
		jdi_mode_switch_to_60(panel);
	} else if (dst_mode == 90) { /* 1200 switch to 60 */
		jdi_mode_switch_to_90(panel);
	} else
		ret = 1;

	return ret;
}

static int jdi_panel_ext_reset(struct drm_panel *panel, int on)
{
	struct jdi *ctx = panel_to_jdi(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}
/*L19 code for HQ-171436 by caogaojie at 2021/12/8 start*/
static struct mtk_panel_funcs ext_funcs = {
	.reset = jdi_panel_ext_reset,
	.set_backlight_cmdq = jdi_setbacklight_cmdq,
	.ext_param_set = jdi_mtk_panel_ext_param_set,
	.ext_param_get = jdi_mtk_panel_ext_param_get,
	.mode_switch = jdi_mode_switch,
	.ata_check = jdi_panel_ata_check,
	.get_panel_info = jdi_panel_get_panel_info,
	.hbm_control = jdi_panel_hbm_control,
};
/*L19 code for HQ-171436 by caogaojie at 2021/12/8 end*/
#endif

struct panel_desc {
	const struct drm_display_mode *modes;
	unsigned int num_modes;

	unsigned int bpc;

	struct {
		unsigned int width;
		unsigned int height;
	} size;

	/**
	 * @prepare: the time (in milliseconds) that it takes for the panel to
	 *	   become ready and start receiving video data
	 * @enable: the time (in milliseconds) that it takes for the panel to
	 *	  display the first valid frame after starting to receive
	 *	  video data
	 * @disable: the time (in milliseconds) that it takes for the panel to
	 *	   turn the display off (no content is visible)
	 * @unprepare: the time (in milliseconds) that it takes for the panel
	 *		 to power itself down completely
	 */
	struct {
		unsigned int prepare;
		unsigned int enable;
		unsigned int disable;
		unsigned int unprepare;
	} delay;
};

static int jdi_get_modes(struct drm_panel *panel)
{
	struct drm_display_mode *mode;
	struct drm_display_mode *mode2;
	/*L19 code for HQ-173138 by caogaojie at 2021/12/21 start*/
	struct drm_display_mode *mode3;
	/*L19 code for HQ-173138 by caogaojie at 2021/12/21 end*/

	mode = drm_mode_duplicate(panel->drm, &default_mode);
	if (!mode) {
		dev_info(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
			 default_mode.hdisplay, default_mode.vdisplay,
			 default_mode.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(panel->connector, mode);

	mode2 = drm_mode_duplicate(panel->drm, &performance_mode);
	if (!mode2) {
		dev_info(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
			 performance_mode.hdisplay, performance_mode.vdisplay,
			 performance_mode.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode2);
	mode2->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(panel->connector, mode2);

	/*L19 code for HQ-173138 by caogaojie at 2021/12/21 start*/
	mode3 = drm_mode_duplicate(panel->drm, &refresh_30_mode);
	if (!mode3) {
		dev_info(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
			 refresh_30_mode.hdisplay, refresh_30_mode.vdisplay,
			 refresh_30_mode.vrefresh);
		return -ENOMEM;
	}
	drm_mode_set_name(mode3);
	mode3->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(panel->connector, mode3);
	/*L19 code for HQ-173138 by caogaojie at 2021/12/21 end*/

	/*L19 code for HQ-159316 by chenzimo at 20211119 start*/
	panel->connector->display_info.width_mm = 68;
	/*L19 code for HQ-159316 by chenzimo at 20211119 end*/
	panel->connector->display_info.height_mm = 152;

	return 1;
}

static const struct drm_panel_funcs jdi_drm_funcs = {
	.disable = jdi_disable,
	.unprepare = jdi_unprepare,
	.prepare = jdi_prepare,
	.enable = jdi_enable,
	.get_modes = jdi_get_modes,
};

static int jdi_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct jdi *ctx;
	struct device_node *backlight;
	int ret;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;

	pr_info("[LCM][KERNEL]ft8722 probe!!!\n");

	if (strstr(saved_command_line, "dsi_panel_l19_31_03_0b_dsc_vdo_lcm_drv")) {
		hq_regiser_hw_info(HWID_LCM, "incell,vendor:31,IC:03");
		pr_err("ft8722 probe match");
	} else {
		pr_err("not match ft8722 !!!\n");
		return -ENODEV;
	}

	dsi_node = of_get_parent(dev->of_node);
	if (dsi_node) {
		endpoint = of_graph_get_next_endpoint(dsi_node, NULL);
		if (endpoint) {
			remote_node = of_graph_get_remote_port_parent(endpoint);
			if (!remote_node) {
				pr_info("No panel connected,skip probe lcm\n");
				return -ENODEV;
			}
			pr_info("device node name:%s\n", remote_node->name);
		}
	}
	if (remote_node != dev->of_node) {
		pr_info("%s+ skip probe due to not current lcm\n", __func__);
		return -ENODEV;
	}

	pr_info("%s+\n", __func__);
	ctx = devm_kzalloc(dev, sizeof(struct jdi), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	lcm_1v3 = regulator_get(NULL, "vcn13");
	if (IS_ERR(lcm_1v3)) {
		ret = PTR_ERR(lcm_1v3);
		pr_err("czmczmczm lcm_1v3!!!\n");
		return ret;
	}

	/*L19 code for HQ-170097 by sunfeiting at 2021/12/2 start*/
	regulator_set_voltage(lcm_1v3, 1300000, 1300000);
	regulator_enable(lcm_1v3);
	/*L19 code for HQ-170097 by sunfeiting at 2021/12/2 end*/

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 3;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
			  MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET |
			  MIPI_DSI_CLOCK_NON_CONTINUOUS;

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(dev, "cannot get reset-gpios %ld\n",
			 PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);
	ctx->bias_pos = devm_gpiod_get_index(dev, "bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_info(dev, "cannot get bias-gpios 0 %ld\n",
			 PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	devm_gpiod_put(dev, ctx->bias_pos);

	ctx->bias_neg = devm_gpiod_get_index(dev, "bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_info(dev, "cannot get bias-gpios 1 %ld\n",
			 PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	devm_gpiod_put(dev, ctx->bias_neg);
	ctx->prepared = true;
	ctx->enabled = true;
	drm_panel_init(&ctx->panel);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &jdi_drm_funcs;
	/*L19 code for HQ-171436 by caogaojie at 2021/12/8 start*/
	ctx->panel_info = panel_name;
	/*L19 code for HQ-171436 by caogaojie at 2021/12/8 end*/

	ret = drm_panel_add(&ctx->panel);
	if (ret < 0)
		return ret;


	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;

#endif
/*L19 code for HQ-172682 by caogaojie at 2021/12/23 start*/
	device_create_file(dev, &dev_attr_mtkfb_fps);
/*L19 code for HQ-172682 by caogaojie at 2021/12/23 end*/

	pr_info("%s- jdi,ft8722,cphy,vdo,90hz,rt4801\n", __func__);

	return ret;
}

static int jdi_remove(struct mipi_dsi_device *dsi)
{
	struct jdi *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id jdi_of_match[] = {
	{
	    .compatible = "ft8722,jdi,vdo",
	},
	{}
};

MODULE_DEVICE_TABLE(of, jdi_of_match);

static struct mipi_dsi_driver jdi_driver = {
	.probe = jdi_probe,
	.remove = jdi_remove,
	.driver = {
		.name = "panel-ft8722-jdi-fhdp-dsi-vdo-90hz",
		.owner = THIS_MODULE,
		.of_match_table = jdi_of_match,
	},
};

module_mipi_dsi_driver(jdi_driver);

MODULE_AUTHOR("Cui Zhang <cui.zhang@mediatek.com>");
MODULE_DESCRIPTION("jdi ft8722 VDO Panel Driver");
MODULE_LICENSE("GPL v2");
