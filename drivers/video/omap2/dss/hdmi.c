/*
 * hdmi.c
 *
 * HDMI interface DSS driver setting for TI's OMAP4 family of processor.
 * Copyright (C) 2010-2011 Texas Instruments Incorporated - http://www.ti.com/
 * Authors: Yong Zhi
 *	Mythri pk <mythripk@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define DSS_SUBSYS_NAME "HDMI"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <video/omapdss.h>
#if defined(CONFIG_OMAP4_DSS_HDMI_AUDIO) || defined(CONFIG_OMAP4_DSS_HDMI_AUDIO)
#include <plat/omap_hwmod.h>
#endif

#include "ti_hdmi.h"
#include "dss.h"
#include "dss_features.h"

#define HDMI_WP			0x0
#define HDMI_PLLCTRL		0x200
#define HDMI_PHY		0x300

#define HDMI_CORE_AV		0x900

/* HDMI EDID Length move this */
#define HDMI_EDID_MAX_LENGTH			256
#define EDID_TIMING_DESCRIPTOR_SIZE		0x12
#define EDID_DESCRIPTOR_BLOCK0_ADDRESS		0x36
#define EDID_DESCRIPTOR_BLOCK1_ADDRESS		0x80
#define EDID_SIZE_BLOCK0_TIMING_DESCRIPTOR	4
#define EDID_SIZE_BLOCK1_TIMING_DESCRIPTOR	4

#define HDMI_DEFAULT_REGN 16
#define HDMI_DEFAULT_REGM2 1

static struct {
	struct mutex lock;
	struct platform_device *pdev;
	struct hdmi_ip_data ip_data;
	int hdmi_irq;

	struct clk *sys_clk;

	struct regulator *vdds_hdmi;

	int ct_cp_hpd_gpio;
	int ls_oe_gpio;
	int hpd_gpio;
} hdmi;

/*
 * Logic for the below structure :
 * user enters the CEA or VESA timings by specifying the HDMI/DVI code.
 * There is a correspondence between CEA/VESA timing and code, please
 * refer to section 6.3 in HDMI 1.3 specification for timing code.
 *
 * In the below structure, cea_vesa_timings corresponds to all OMAP4
 * supported CEA and VESA timing values.code_cea corresponds to the CEA
 * code, It is used to get the timing from cea_vesa_timing array.Similarly
 * with code_vesa. Code_index is used for back mapping, that is once EDID
 * is read from the TV, EDID is parsed to find the timing values and then
 * map it to corresponding CEA or VESA index.
 */

static const struct hdmi_config cea_timings[] = {
{ {640, 480, 25200, 96, 16, 48, 2, 10, 33, 0, 0, 0}, {1, HDMI_HDMI} },
{ {720, 480, 27027, 62, 16, 60, 6, 9, 30, 0, 0, 0}, {2, HDMI_HDMI} },
{ {1280, 720, 74250, 40, 110, 220, 5, 5, 20, 1, 1, 0}, {4, HDMI_HDMI} },
{ {1920, 540, 74250, 44, 88, 148, 5, 2, 15, 1, 1, 1}, {5, HDMI_HDMI} },
{ {1440, 240, 27027, 124, 38, 114, 3, 4, 15, 0, 0, 1}, {6, HDMI_HDMI} },
{ {1920, 1080, 148500, 44, 88, 148, 5, 4, 36, 1, 1, 0}, {16, HDMI_HDMI} },
{ {720, 576, 27000, 64, 12, 68, 5, 5, 39, 0, 0, 0}, {17, HDMI_HDMI} },
{ {1280, 720, 74250, 40, 440, 220, 5, 5, 20, 1, 1, 0}, {19, HDMI_HDMI} },
{ {1920, 540, 74250, 44, 528, 148, 5, 2, 15, 1, 1, 1}, {20, HDMI_HDMI} },
{ {1440, 288, 27000, 126, 24, 138, 3, 2, 19, 0, 0, 1}, {21, HDMI_HDMI} },
{ {1440, 576, 54000, 128, 24, 136, 5, 5, 39, 0, 0, 0}, {29, HDMI_HDMI} },
{ {1920, 1080, 148500, 44, 528, 148, 5, 4, 36, 1, 1, 0}, {31, HDMI_HDMI} },
{ {1920, 1080, 74250, 44, 638, 148, 5, 4, 36, 1, 1, 0}, {32, HDMI_HDMI} },
{ {2880, 480, 108108, 248, 64, 240, 6, 9, 30, 0, 0, 0}, {35, HDMI_HDMI} },
{ {2880, 576, 108000, 256, 48, 272, 5, 5, 39, 0, 0, 0}, {37, HDMI_HDMI} },
};
static const struct hdmi_config vesa_timings[] = {
/* VESA From Here */
{ {640, 480, 25175, 96, 16, 48, 2 , 11, 31, 0, 0, 0}, {4, HDMI_DVI} },
{ {800, 600, 40000, 128, 40, 88, 4 , 1, 23, 1, 1, 0}, {9, HDMI_DVI} },
{ {848, 480, 33750, 112, 16, 112, 8 , 6, 23, 1, 1, 0}, {0xE, HDMI_DVI} },
{ {1280, 768, 79500, 128, 64, 192, 7 , 3, 20, 1, 0, 0}, {0x17, HDMI_DVI} },
{ {1280, 800, 83500, 128, 72, 200, 6 , 3, 22, 1, 0, 0}, {0x1C, HDMI_DVI} },
{ {1360, 768, 85500, 112, 64, 256, 6 , 3, 18, 1, 1, 0}, {0x27, HDMI_DVI} },
{ {1280, 960, 108000, 112, 96, 312, 3 , 1, 36, 1, 1, 0}, {0x20, HDMI_DVI} },
{ {1280, 1024, 108000, 112, 48, 248, 3 , 1, 38, 1, 1, 0}, {0x23, HDMI_DVI} },
{ {1024, 768, 65000, 136, 24, 160, 6, 3, 29, 0, 0, 0}, {0x10, HDMI_DVI} },
{ {1400, 1050, 121750, 144, 88, 232, 4, 3, 32, 1, 0, 0}, {0x2A, HDMI_DVI} },
{ {1440, 900, 106500, 152, 80, 232, 6, 3, 25, 1, 0, 0}, {0x2F, HDMI_DVI} },
{ {1680, 1050, 146250, 176 , 104, 280, 6, 3, 30, 1, 0, 0}, {0x3A, HDMI_DVI} },
{ {1366, 768, 85500, 143, 70, 213, 3, 3, 24, 1, 1, 0}, {0x51, HDMI_DVI} },
{ {1920, 1080, 148500, 44, 148, 80, 5, 4, 36, 1, 1, 0}, {0x52, HDMI_DVI} },
{ {1280, 768, 68250, 32, 48, 80, 7, 3, 12, 0, 1, 0}, {0x16, HDMI_DVI} },
{ {1400, 1050, 101000, 32, 48, 80, 4, 3, 23, 0, 1, 0}, {0x29, HDMI_DVI} },
{ {1680, 1050, 119000, 32, 48, 80, 6, 3, 21, 0, 1, 0}, {0x39, HDMI_DVI} },
{ {1280, 800, 79500, 32, 48, 80, 6, 3, 14, 0, 1, 0}, {0x1B, HDMI_DVI} },
{ {1280, 720, 74250, 40, 110, 220, 5, 5, 20, 1, 1, 0}, {0x55, HDMI_DVI} }
};

static const struct hdmi_config s3d_timings[] = {
{ {1280, 1485, 148500, 40, 110, 220, 5, 5, 20, 1, 1, 0}, {4, HDMI_HDMI} },
{ {1280, 1485, 148500, 40, 440, 220, 5, 5, 20, 1, 1, 0}, {19, HDMI_HDMI} },
{ {1920, 2205, 148500, 44, 638, 148, 5, 4, 36, 1, 1, 0}, {32, HDMI_HDMI} },
};

static int hdmi_runtime_get(void)
{
	int r;

	DSSDBG("hdmi_runtime_get\n");

	if (!pm_runtime_enabled(&hdmi.pdev->dev))
		return 0;

	r = pm_runtime_get_sync(&hdmi.pdev->dev);
	WARN_ON(r < 0);
	if (r < 0) {
		DSSERR("%s error: %d\n", __func__, r);
		return r;
	}

	return 0;
}

static void hdmi_runtime_put(void)
{
	int r;

	DSSDBG("hdmi_runtime_put\n");

	if (!pm_runtime_enabled(&hdmi.pdev->dev))
		return;

	r = pm_runtime_put_sync(&hdmi.pdev->dev);
	WARN_ON(r < 0);
	if (r < 0)
		DSSERR("%s error: %d\n", __func__, r);
}

static irqreturn_t hpd_irq_handler(int irq, void *data)
{
	struct omap_dss_device *dssdev = data;
	enum omap_dss_event evt;
	int state;

	DSSDBG("%s\n", __func__);
	if (hdmi_runtime_get())
		return IRQ_HANDLED;

	state = hdmi.ip_data.ops->detect(&hdmi.ip_data);

	/*
	 * To prevent possible HW damage, HPD interrupt handler must
	 * call detect() as soon as there is a change in HPD level.
	 * The call to hdmi_check_hpd_state() ensures that the power
	 * level to HDMI IP is safe.
	 */
	hdmi.ip_data.ops->check_hpd_state(&hdmi.ip_data);

	hdmi_runtime_put();

	DSSDBG("%s hpd: %d\n", __func__, state);
	if (state)
		evt = OMAP_DSS_EVENT_HOTPLUG_CONNECT;
	else
		evt = OMAP_DSS_EVENT_HOTPLUG_DISCONNECT;

	dss_notify(dssdev, evt);

	return IRQ_HANDLED;
}

static int __init hdmi_init_display(struct omap_dss_device *dssdev)
{
	int r;

	struct gpio gpios[] = {
		/* CT_CP_HPD_GPIO must be always ON for HPD detection to work */
		{ hdmi.ct_cp_hpd_gpio, GPIOF_OUT_INIT_HIGH, "hdmi_ct_cp_hpd" },
		{ hdmi.ls_oe_gpio, GPIOF_OUT_INIT_LOW, "hdmi_ls_oe" },
		{ hdmi.hpd_gpio, GPIOF_DIR_IN, "hdmi_hpd" },
	};

	DSSDBG("init_display\n");

	dss_init_hdmi_ip_ops(&hdmi.ip_data);

	r = gpio_request_array(gpios, ARRAY_SIZE(gpios));
	if (r)
		return r;

	if (gpio_is_valid(hdmi.hpd_gpio)) {
		r = request_threaded_irq(gpio_to_irq(hdmi.hpd_gpio),
				NULL, hpd_irq_handler,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING |
				IRQF_ONESHOT, "hpd", dssdev);
		if (r)
			DSSERR("%s: request hpd irq failed. "
			  "HDMI detection will be polled only.\n", __func__);
	} else {
		DSSERR("%s: no valid HPD gpio. "
			"HDMI detection will be polled only.\n", __func__);
	}

	return 0;
}

static void __exit hdmi_uninit_display(struct omap_dss_device *dssdev)
{
	DSSDBG("uninit_display\n");

	if (gpio_is_valid(hdmi.hpd_gpio))
		free_irq(gpio_to_irq(hdmi.hpd_gpio), dssdev);

	gpio_free(hdmi.ct_cp_hpd_gpio);
	gpio_free(hdmi.ls_oe_gpio);
	gpio_free(hdmi.hpd_gpio);
}

static const struct hdmi_config *hdmi_find_timing(
					const struct hdmi_config *timings_arr,
					int len)
{
	int i;

	for (i = 0; i < len; i++) {
		if (timings_arr[i].cm.code == hdmi.ip_data.cfg.cm.code)
			return &timings_arr[i];
	}
	return NULL;
}

static const struct hdmi_config *hdmi_get_timings(void)
{
       const struct hdmi_config *arr;
       int len;


	if (!hdmi.ip_data.cfg.s3d_enabled) {
		if (hdmi.ip_data.cfg.cm.mode == HDMI_DVI) {
			arr = vesa_timings;
			len = ARRAY_SIZE(vesa_timings);
		} else {
			arr = cea_timings;
			len = ARRAY_SIZE(cea_timings);
		}
	} else {
		arr = s3d_timings;
		len = ARRAY_SIZE(s3d_timings);
	}
	return hdmi_find_timing(arr, len);
}

static bool hdmi_timings_compare(struct omap_video_timings *timing1,
				const struct hdmi_video_timings *timing2)
{
	int timing1_vsync, timing1_hsync, timing2_vsync, timing2_hsync;

	if ((timing2->pixel_clock == timing1->pixel_clock) &&
		(timing2->x_res == timing1->x_res) &&
		(timing2->y_res == timing1->y_res)) {

		timing2_hsync = timing2->hfp + timing2->hsw + timing2->hbp;
		timing1_hsync = timing1->hfp + timing1->hsw + timing1->hbp;
		timing2_vsync = timing2->vfp + timing2->vsw + timing2->vbp;
		timing1_vsync = timing2->vfp + timing2->vsw + timing2->vbp;

		DSSDBG("timing1_hsync = %d timing1_vsync = %d"\
			"timing2_hsync = %d timing2_vsync = %d\n",
			timing1_hsync, timing1_vsync,
			timing2_hsync, timing2_vsync);

		if ((timing1_hsync == timing2_hsync) &&
			(timing1_vsync == timing2_vsync)) {
			return true;
		}
	}
	return false;
}

static struct hdmi_cm hdmi_get_code(struct omap_video_timings *timing)
{
	int i;
	struct hdmi_cm cm = {-1};
	DSSDBG("hdmi_get_code\n");

	for (i = 0; i < ARRAY_SIZE(cea_timings); i++) {
		if (hdmi_timings_compare(timing, &cea_timings[i].timings)) {
			cm = cea_timings[i].cm;
			goto end;
		}
	}
	for (i = 0; i < ARRAY_SIZE(vesa_timings); i++) {
		if (hdmi_timings_compare(timing, &vesa_timings[i].timings)) {
			cm = vesa_timings[i].cm;
			goto end;
		}
	}
	for (i = 0; i < ARRAY_SIZE(s3d_timings); i++) {
		if (hdmi_timings_compare(timing, &s3d_timings[i].timings)) {
			cm = s3d_timings[i].cm;
			goto end;
		}
	}

end:	return cm;

}

unsigned long hdmi_get_pixel_clock(void)
{
	/* HDMI Pixel Clock in Mhz */
	return hdmi.ip_data.cfg.timings.pixel_clock * 1000;
}

static void hdmi_compute_pll(struct omap_dss_device *dssdev, int phy,
		struct hdmi_pll_info *pi)
{
	unsigned long clkin, refclk;
	u32 mf;

	clkin = clk_get_rate(hdmi.sys_clk) / 10000;
	/*
	 * Input clock is predivided by N + 1
	 * out put of which is reference clk
	 */
	if (dssdev->clocks.hdmi.regn == 0)
		pi->regn = HDMI_DEFAULT_REGN;
	else
		pi->regn = dssdev->clocks.hdmi.regn;

	refclk = clkin / pi->regn;

	if (dssdev->clocks.hdmi.regm2 == 0) {
		if (cpu_is_omap44xx()) {
			pi->regm2 = HDMI_DEFAULT_REGM2;
		} else if (cpu_is_omap54xx()) {
			if (phy <= 50000)
				pi->regm2 = 2;
			else
				pi->regm2 = 1;
		}
	} else {
		pi->regm2 = dssdev->clocks.hdmi.regm2;
	}

	/*
	 * multiplier is pixel_clk/ref_clk
	 * Multiplying by 100 to avoid fractional part removal
	 */
	pi->regm = phy * pi->regm2 / refclk;

	/*
	 * fractional multiplier is remainder of the difference between
	 * multiplier and actual phy(required pixel clock thus should be
	 * multiplied by 2^18(262144) divided by the reference clock
	 */
	mf = (phy - pi->regm / pi->regm2 * refclk) * 262144;
	pi->regmf = pi->regm2 * mf / refclk;

	/*
	 * Dcofreq should be set to 1 if required pixel clock
	 * is greater than 1000MHz
	 */
	pi->dcofreq = phy > 1000 * 100;
	pi->regsd = ((pi->regm * clkin / 10) / (pi->regn * 250) + 5) / 10;

	/* Set the reference clock to sysclk reference */
	pi->refsel = HDMI_REFSEL_SYSCLK;

	DSSDBG("M = %d Mf = %d\n", pi->regm, pi->regmf);
	DSSDBG("range = %d sd = %d\n", pi->dcofreq, pi->regsd);
}

static int hdmi_power_on(struct omap_dss_device *dssdev)
{
	int r;
	const struct hdmi_config *timing;
	struct omap_video_timings *p;
	unsigned long phy;

	gpio_set_value(hdmi.ls_oe_gpio, 1);

	r = hdmi_runtime_get();
	if (r)
		goto err_runtime_get;

	if (cpu_is_omap54xx()) {
		r = regulator_enable(hdmi.vdds_hdmi);
		if (r)
			goto err_regulator;
	}

	dss_mgr_disable(dssdev->manager);

	p = &dssdev->panel.timings;

	DSSDBG("hdmi_power_on x_res= %d y_res = %d\n",
		dssdev->panel.timings.x_res,
		dssdev->panel.timings.y_res);

	timing = hdmi_get_timings();
	if (timing == NULL) {
		/* HDMI code 4 corresponds to 1024 * 768 VGA */
		hdmi.ip_data.cfg.cm.code = 0x10;
		/* DVI mode 1 corresponds to HDMI 0 to DVI */
		hdmi.ip_data.cfg.cm.mode = HDMI_DVI;
		hdmi.ip_data.cfg = vesa_timings[8];
	} else {
		hdmi.ip_data.cfg = *timing;
	}

	switch (hdmi.ip_data.cfg.deep_color) {
	case HDMI_DEEP_COLOR_30BIT:
		phy = (p->pixel_clock * 125) / 100 ;
		break;
	case HDMI_DEEP_COLOR_36BIT:
		if (p->pixel_clock >= 148500) {
			DSSERR("36 bit deep color not supported for the \
				pixel clock %d\n", p->pixel_clock);
			goto err_pll_enable;
		}
		phy = (p->pixel_clock * 150) / 100;
		break;
	case HDMI_DEEP_COLOR_24BIT:
	default:
		phy = p->pixel_clock;
		break;
	}

	hdmi_compute_pll(dssdev, phy, &hdmi.ip_data.pll_data);

	hdmi.ip_data.ops->video_disable(&hdmi.ip_data);

	/* config the PLL and PHY hdmi_set_pll_pwrfirst */
	r = hdmi.ip_data.ops->pll_enable(&hdmi.ip_data);
	if (r) {
		DSSDBG("Failed to lock PLL\n");
		goto err_pll_enable;
	}

	r = hdmi.ip_data.ops->phy_enable(&hdmi.ip_data);
	if (r) {
		DSSDBG("Failed to start PHY\n");
		goto err_phy_enable;
	}

	hdmi.ip_data.ops->video_configure(&hdmi.ip_data);

	/* Make selection of HDMI in DSS */
	dss_select_hdmi_venc_clk_source(DSS_HDMI_M_PCLK);

	/* Select the dispc clock source as PRCM clock, to ensure that it is not
	 * DSI PLL source as the clock selected by DSI PLL might not be
	 * sufficient for the resolution selected / that can be changed
	 * dynamically by user. This can be moved to single location , say
	 * Boardfile.
	 */
	dss_select_dispc_clk_source(dssdev->clocks.dispc.dispc_fclk_src);

	if (dssdev->clocks.fck_div) {
		struct dss_clock_info dss_cinfo;

		memset(&dss_cinfo, 0, sizeof(dss_cinfo));

		dss_cinfo.fck_div = dssdev->clocks.fck_div;

		r = dss_calc_clock_rates(&dss_cinfo);
		if (r) {
			printk("calc failed\n");
			return r;
		}

		r = dss_set_clock_div(&dss_cinfo);
		if (r) {
			printk("set failed\n");
			return r;
		}
	}

	/* bypass TV gamma table */
	dispc_enable_gamma_table(0);

	/* tv size */
	dss_mgr_set_timings(dssdev->manager, &dssdev->panel.timings);

	r = hdmi.ip_data.ops->video_enable(&hdmi.ip_data);
	if (r)
		goto err_vid_enable;

	r = dss_mgr_enable(dssdev->manager);
	if (r)
		goto err_mgr_enable;

	return 0;

err_mgr_enable:
	hdmi.ip_data.ops->video_disable(&hdmi.ip_data);
err_vid_enable:
	hdmi.ip_data.ops->phy_disable(&hdmi.ip_data);
err_phy_enable:
	hdmi.ip_data.ops->pll_disable(&hdmi.ip_data);
err_pll_enable:
	if (cpu_is_omap54xx())
		regulator_disable(hdmi.vdds_hdmi);
err_regulator:
	hdmi_runtime_put();
err_runtime_get:
	gpio_set_value(hdmi.ls_oe_gpio, 0);
	return -EIO;
}

static void hdmi_power_off(struct omap_dss_device *dssdev)
{
	dss_mgr_disable(dssdev->manager);

	hdmi.ip_data.ops->video_disable(&hdmi.ip_data);
	hdmi.ip_data.ops->phy_disable(&hdmi.ip_data);
	hdmi.ip_data.ops->pll_disable(&hdmi.ip_data);

	if (cpu_is_omap54xx())
		regulator_disable(hdmi.vdds_hdmi);

	hdmi_runtime_put();

	gpio_set_value(hdmi.ls_oe_gpio, 0);

	hdmi.ip_data.cfg.deep_color = HDMI_DEEP_COLOR_24BIT;
}

int omapdss_hdmi_set_deepcolor(struct omap_dss_device *dssdev, int val,
		bool hdmi_restart)
{
	int r;

	if (!hdmi_restart) {
		hdmi.ip_data.cfg.deep_color = val;
		return 0;
	}

	omapdss_hdmi_display_disable(dssdev);

	hdmi.ip_data.cfg.deep_color = val;

	r = omapdss_hdmi_display_enable(dssdev);
	if (r)
		return r;

	return 0;
}

int omapdss_hdmi_get_deepcolor(void)
{
	return hdmi.ip_data.cfg.deep_color;
}

int omapdss_hdmi_set_range(int range)
{
	int r = 0;
	enum hdmi_range old_range;

	old_range = hdmi.ip_data.cfg.range;
	hdmi.ip_data.cfg.range = range;

	/* HDMI 1.3 section 6.6 VGA (640x480) format requires Full Range */
	if ((range == 0) &&
		((hdmi.ip_data.cfg.cm.code == 4 &&
		hdmi.ip_data.cfg.cm.mode == HDMI_DVI) ||
		(hdmi.ip_data.cfg.cm.code == 1 &&
		hdmi.ip_data.cfg.cm.mode == HDMI_HDMI)))
			return -EINVAL;

	r = hdmi.ip_data.ops->configure_range(&hdmi.ip_data);
	if (r)
		hdmi.ip_data.cfg.range = old_range;

	return r;
}

int omapdss_hdmi_get_range(void)
{
	return hdmi.ip_data.cfg.range;
}

int omapdss_hdmi_display_check_timing(struct omap_dss_device *dssdev,
					struct omap_video_timings *timings)
{
	struct hdmi_cm cm;

	cm = hdmi_get_code(timings);
	if (cm.code == -1 || (timings->pixel_clock >
		dss_feat_get_param_max(FEAT_PARAM_HDMI_MAXPCLK))) {
			return -EINVAL;
	}

	return 0;

}

int omapdss_hdmi_display_3d_enable(struct omap_dss_device *dssdev,
					struct s3d_disp_info *info, int code)
{
	int r = 0;

	DSSDBG("ENTER hdmi_display_3d_enable\n");

	mutex_lock(&hdmi.lock);

	if (dssdev->manager == NULL) {
		DSSERR("failed to enable display: no manager\n");
		r = -ENODEV;
		goto err0;
	}

	r = omap_dss_start_device(dssdev);
	if (r) {
		DSSERR("failed to start device\n");
		goto err0;
	}

	/* hdmi.s3d_enabled will be updated when powering display up */
	/* if there's no S3D support it will be reset to false */
	switch (info->type) {
	case S3D_DISP_OVERUNDER:
		if (info->sub_samp == S3D_DISP_SUB_SAMPLE_NONE) {
			dssdev->panel.s3d_info = *info;
			hdmi.ip_data.cfg.s3d_info.frame_struct = HDMI_S3D_FRAME_PACKING;
			hdmi.ip_data.cfg.s3d_info.subsamp = false;
			hdmi.ip_data.cfg.s3d_info.subsamp_pos = 0;
			hdmi.ip_data.cfg.s3d_enabled = true;
			hdmi.ip_data.cfg.s3d_info.vsi_enabled = true;
		} else {
			goto err1;
		}
		break;
	case S3D_DISP_SIDEBYSIDE:
		dssdev->panel.s3d_info = *info;
		if (info->sub_samp == S3D_DISP_SUB_SAMPLE_NONE) {
			hdmi.ip_data.cfg.s3d_info.frame_struct = HDMI_S3D_SIDE_BY_SIDE_FULL;
			hdmi.ip_data.cfg.s3d_info.subsamp = true;
			hdmi.ip_data.cfg.s3d_info.subsamp_pos = HDMI_S3D_HOR_EL_ER;
			hdmi.ip_data.cfg.s3d_enabled = true;
			hdmi.ip_data.cfg.s3d_info.vsi_enabled = true;
		} else if (info->sub_samp == S3D_DISP_SUB_SAMPLE_H) {
			hdmi.ip_data.cfg.s3d_info.frame_struct = HDMI_S3D_SIDE_BY_SIDE_HALF;
			hdmi.ip_data.cfg.s3d_info.subsamp = true;
			hdmi.ip_data.cfg.s3d_info.subsamp_pos = HDMI_S3D_HOR_EL_ER;
			hdmi.ip_data.cfg.s3d_info.vsi_enabled = true;
		} else {
			goto err1;
		}
		break;
	default:
		goto err1;
	}
	if (hdmi.ip_data.cfg.s3d_enabled) {
		hdmi.ip_data.cfg.cm.code = code;
		hdmi.ip_data.cfg.cm.mode = HDMI_HDMI;
	}

	r = hdmi_power_on(dssdev);
	if (r) {
		DSSERR("failed to power on device\n");
		goto err1;
	}

	mutex_unlock(&hdmi.lock);
	return 0;

err1:
	omap_dss_stop_device(dssdev);
err0:
	mutex_unlock(&hdmi.lock);
	return r;
}

void omapdss_hdmi_display_set_timing(struct omap_dss_device *dssdev)
{
	struct hdmi_cm cm;

	cm = hdmi_get_code(&dssdev->panel.timings);
	if (cm.code == -1) {
		hdmi.ip_data.cfg.cm.code = 0;
		/* Assume VESA timing if non-standard */
		hdmi.ip_data.cfg.cm.mode = 0;
	} else {
		hdmi.ip_data.cfg.cm.code = cm.code;
		hdmi.ip_data.cfg.cm.mode = cm.mode;
	};

	if (dssdev->state == OMAP_DSS_DISPLAY_ACTIVE) {
		int r;

		hdmi_power_off(dssdev);

		r = hdmi_power_on(dssdev);
		if (r)
			DSSERR("failed to power on device\n");
	} else {
		dss_mgr_set_timings(dssdev->manager, &dssdev->panel.timings);
	}

//	if (cpu_is_omap54xx())
  //      	omapdss_hdmi_display_enable(dssdev);                                    
}

static void hdmi_dump_regs(struct seq_file *s)
{
	mutex_lock(&hdmi.lock);

	if (hdmi_runtime_get())
		return;

	hdmi.ip_data.ops->dump_wrapper(&hdmi.ip_data, s);
	hdmi.ip_data.ops->dump_pll(&hdmi.ip_data, s);
	hdmi.ip_data.ops->dump_phy(&hdmi.ip_data, s);
	hdmi.ip_data.ops->dump_core(&hdmi.ip_data, s);

	hdmi_runtime_put();
	mutex_unlock(&hdmi.lock);
}

int omapdss_hdmi_read_edid(u8 *buf, int len)
{
	int r;

	mutex_lock(&hdmi.lock);

	r = hdmi_runtime_get();
	BUG_ON(r);

	r = hdmi.ip_data.ops->read_edid(&hdmi.ip_data, buf, len);

	hdmi_runtime_put();
	mutex_unlock(&hdmi.lock);

	return r;
}

bool omapdss_hdmi_detect(void)
{
	int r;

	mutex_lock(&hdmi.lock);

	r = hdmi_runtime_get();
	BUG_ON(r);

	r = 1;
	if (hdmi.ip_data.ops->detect)
		r = hdmi.ip_data.ops->detect(&hdmi.ip_data);

	hdmi_runtime_put();
	mutex_unlock(&hdmi.lock);

	return r == 1;
}

int omapdss_hdmi_display_enable(struct omap_dss_device *dssdev)
{
	int r = 0;

	DSSDBG("ENTER hdmi_display_enable\n");

	mutex_lock(&hdmi.lock);

	if (dssdev->manager == NULL) {
		DSSERR("failed to enable display: no manager\n");
		r = -ENODEV;
		goto err0;
	}

	r = omap_dss_start_device(dssdev);
	if (r) {
		DSSERR("failed to start device\n");
		goto err0;
	}

	r = hdmi_power_on(dssdev);
	if (r) {
		DSSERR("failed to power on device\n");
		goto err1;
	}

	mutex_unlock(&hdmi.lock);
	return 0;

err1:
	omap_dss_stop_device(dssdev);
err0:
	mutex_unlock(&hdmi.lock);
	return r;
}

void omapdss_hdmi_display_disable(struct omap_dss_device *dssdev)
{
	DSSDBG("Enter hdmi_display_disable\n");

	mutex_lock(&hdmi.lock);

	hdmi_power_off(dssdev);

	omap_dss_stop_device(dssdev);

	mutex_unlock(&hdmi.lock);
}

static irqreturn_t hdmi_irq_handler(int irq, void *arg)
{
	DSSDBG("Received HDMI IRQ\n");

	if (hdmi_runtime_get())
		return IRQ_HANDLED;

	if (hdmi.ip_data.ops->irq_handler)
		hdmi.ip_data.ops->irq_handler(&hdmi.ip_data);

	if (hdmi.ip_data.ops->irq_process)
		hdmi.ip_data.ops->irq_process(&hdmi.ip_data);

	hdmi_runtime_put();

	return IRQ_HANDLED;
}

static int hdmi_get_clocks(struct platform_device *pdev)
{
	struct clk *clk;

	clk = clk_get(&pdev->dev, "sys_clk");
	if (IS_ERR(clk)) {
		DSSERR("can't get sys_clk\n");
		return PTR_ERR(clk);
	}

	hdmi.sys_clk = clk;

	return 0;
}

static void hdmi_put_clocks(void)
{
	if (hdmi.sys_clk)
		clk_put(hdmi.sys_clk);
}

#if defined(CONFIG_OMAP4_DSS_HDMI_AUDIO)
int hdmi_compute_acr(u32 sample_freq, u32 *n, u32 *cts)
{
	u32 deep_color;
	bool deep_color_correct = false;
	u32 pclk = hdmi.ip_data.cfg.timings.pixel_clock;

	if (n == NULL || cts == NULL)
		return -EINVAL;

	/* TODO: When implemented, query deep color mode here. */
	deep_color = 100;

	/*
	 * When using deep color, the default N value (as in the HDMI
	 * specification) yields to an non-integer CTS. Hence, we
	 * modify it while keeping the restrictions described in
	 * section 7.2.1 of the HDMI 1.4a specification.
	 */
	switch (sample_freq) {
	case 32000:
	case 48000:
	case 96000:
	case 192000:
		if (deep_color == 125)
			if (pclk == 27027 || pclk == 74250)
				deep_color_correct = true;
		if (deep_color == 150)
			if (pclk == 27027)
				deep_color_correct = true;
		break;
	case 44100:
	case 88200:
	case 176400:
		if (deep_color == 125)
			if (pclk == 27027)
				deep_color_correct = true;
		break;
	default:
		return -EINVAL;
	}

	if (deep_color_correct) {
		switch (sample_freq) {
		case 32000:
			*n = 8192;
			break;
		case 44100:
			*n = 12544;
			break;
		case 48000:
			*n = 8192;
			break;
		case 88200:
			*n = 25088;
			break;
		case 96000:
			*n = 16384;
			break;
		case 176400:
			*n = 50176;
			break;
		case 192000:
			*n = 32768;
			break;
		default:
			return -EINVAL;
		}
	} else {
		switch (sample_freq) {
		case 32000:
			*n = 4096;
			break;
		case 44100:
			*n = 6272;
			break;
		case 48000:
			*n = 6144;
			break;
		case 88200:
			*n = 12544;
			break;
		case 96000:
			*n = 12288;
			break;
		case 176400:
			*n = 25088;
			break;
		case 192000:
			*n = 24576;
			break;
		default:
			return -EINVAL;
		}
	}
	/* Calculate CTS. See HDMI 1.3a or 1.4a specifications */
	*cts = pclk * (*n / 128) * deep_color / (sample_freq / 10);

	return 0;
}

int hdmi_audio_enable(void)
{
	int r;

	DSSDBG("audio_enable\n");

	r = hdmi_runtime_get();
	BUG_ON(r);

	r = hdmi.ip_data.ops->audio_enable(&hdmi.ip_data);
	hdmi_runtime_put();

	return r;
}

void hdmi_audio_disable(void)
{
	int r;

	DSSDBG("audio_disable\n");

	r = hdmi_runtime_get();
	BUG_ON(r);

	hdmi.ip_data.ops->audio_disable(&hdmi.ip_data);
	hdmi_runtime_put();
}

int hdmi_audio_start(void)
{
	int r;

	DSSDBG("audio_start\n");

	r = hdmi_runtime_get();
	BUG_ON(r);

	r = hdmi.ip_data.ops->audio_start(&hdmi.ip_data);
	hdmi_runtime_put();

	return r;
}

void hdmi_audio_stop(void)
{
	int r;

	DSSDBG("audio_stop\n");

	r = hdmi_runtime_get();
	BUG_ON(r);

	hdmi.ip_data.ops->audio_stop(&hdmi.ip_data);
	hdmi_runtime_put();
}

bool hdmi_mode_has_audio(void)
{
	if (hdmi.ip_data.cfg.cm.mode == HDMI_HDMI)
		return true;
	else
		return false;
}

int hdmi_audio_config(struct omap_dss_audio *audio)
{
	int r;

	r = hdmi_runtime_get();
	BUG_ON(r);

	r = hdmi.ip_data.ops->audio_config(&hdmi.ip_data, audio);
	hdmi_runtime_put();

	return r;
}

#endif

static void __init hdmi_probe_pdata(struct platform_device *pdev)
{
	struct omap_dss_board_info *pdata = pdev->dev.platform_data;
	int r, i;

	for (i = 0; i < pdata->num_devices; ++i) {
		struct omap_dss_device *dssdev = pdata->devices[i];
		struct omap_dss_hdmi_data *priv = dssdev->data;

		if (dssdev->type != OMAP_DISPLAY_TYPE_HDMI)
			continue;

		hdmi.ip_data.hpd_gpio = priv->hpd_gpio;
		hdmi.ct_cp_hpd_gpio = priv->ct_cp_hpd_gpio;
		hdmi.ls_oe_gpio = priv->ls_oe_gpio;
		hdmi.hpd_gpio = priv->hpd_gpio;

		r = hdmi_init_display(dssdev);
		if (r) {
			DSSERR("device %s init failed: %d\n", dssdev->name, r);
			continue;
		}

		r = omap_dss_register_device(dssdev, &pdev->dev, i);
		if (r)
			DSSERR("device %s register failed: %d\n",
					dssdev->name, r);
	}
}

/* HDMI HW IP initialisation */
static int __init omapdss_hdmihw_probe(struct platform_device *pdev)
{
	struct resource *hdmi_mem;
	struct regulator *vdds_hdmi;
	int r;

	hdmi.pdev = pdev;

	mutex_init(&hdmi.lock);

	pio_a_init();

	hdmi_mem = platform_get_resource(hdmi.pdev, IORESOURCE_MEM, 0);
	if (!hdmi_mem) {
		DSSERR("can't get IORESOURCE_MEM HDMI\n");
		return -EINVAL;
	}

	/* Base address taken from platform */
	hdmi.ip_data.base_wp = ioremap(hdmi_mem->start,
						resource_size(hdmi_mem));
	if (!hdmi.ip_data.base_wp) {
		DSSERR("can't ioremap WP\n");
		return -ENOMEM;
	}

	r = hdmi_get_clocks(pdev);
	if (r) {
		iounmap(hdmi.ip_data.base_wp);
		return r;
	}

#if defined(CONFIG_OMAP4_DSS_HDMI_AUDIO) || defined(CONFIG_OMAP5_DSS_HDMI_AUDIO)
	/* obtain HDMI hwmod data */
	hdmi.ip_data.oh = omap_hwmod_lookup("dss_hdmi");
	if (!hdmi.ip_data.oh) {
		DSSERR("can't get HDMI hwmod data\n");
		hdmi_put_clocks();
		iounmap(hdmi.ip_data.base_wp);
		return -ENODEV;
	}
#endif

	pm_runtime_enable(&pdev->dev);

	hdmi.hdmi_irq = platform_get_irq(pdev, 0);
	r = request_irq(hdmi.hdmi_irq, hdmi_irq_handler, 0, "OMAP HDMI", NULL);
	if (r < 0) {
		pr_err("hdmi: request_irq %s failed\n", pdev->name);
		return -EINVAL;
	}

	if (cpu_is_omap54xx()) {
		/* Request for regulator supply required by HDMI PHY */
		vdds_hdmi = regulator_get(&pdev->dev, "vdds_hdmi");
		if (IS_ERR(vdds_hdmi)) {
			DSSERR("can't get VDDS_HDMI regulator\n");
			return PTR_ERR(vdds_hdmi);
		}
	hdmi.vdds_hdmi = vdds_hdmi;
	}

	hdmi.ip_data.core_sys_offset = dss_feat_get_hdmi_core_sys_offset();
	hdmi.ip_data.core_av_offset = HDMI_CORE_AV;
	hdmi.ip_data.pll_offset = HDMI_PLLCTRL;
	hdmi.ip_data.phy_offset = HDMI_PHY;

	hdmi_panel_init();

	dss_debugfs_create_file("hdmi", hdmi_dump_regs);

	hdmi_probe_pdata(pdev);

	return 0;
}

static int __exit hdmi_remove_child(struct device *dev, void *data)
{
	struct omap_dss_device *dssdev = to_dss_device(dev);
	hdmi_uninit_display(dssdev);
	return 0;
}

static int __exit omapdss_hdmihw_remove(struct platform_device *pdev)
{
	device_for_each_child(&pdev->dev, NULL, hdmi_remove_child);

	omap_dss_unregister_child_devices(&pdev->dev);

	hdmi_panel_exit();

	pm_runtime_disable(&pdev->dev);

	hdmi_put_clocks();

	if (cpu_is_omap54xx()) {
		regulator_put(hdmi.vdds_hdmi);
		hdmi.vdds_hdmi = NULL;
	}

	pio_a_exit();

	iounmap(hdmi.ip_data.base_wp);

	return 0;
}

static int hdmi_runtime_suspend(struct device *dev)
{
	clk_disable(hdmi.sys_clk);

	dispc_runtime_put();

	return 0;
}

static int hdmi_runtime_resume(struct device *dev)
{
	int r;

	r = dispc_runtime_get();
	if (r < 0)
		return r;

	clk_enable(hdmi.sys_clk);

	return 0;
}

static int hdmi_suspend(struct device *dev)
{
	DSSDBG("%s\n", __func__);
	if (gpio_is_valid(hdmi.hpd_gpio))
		disable_irq(gpio_to_irq(hdmi.hpd_gpio));

	disable_irq(hdmi.hdmi_irq);
	return 0;
}

static int hdmi_resume(struct device *dev)
{
	DSSDBG("%s\n", __func__);
	if (gpio_is_valid(hdmi.hpd_gpio))
		enable_irq(gpio_to_irq(hdmi.hpd_gpio));

	enable_irq(hdmi.hdmi_irq);
	return 0;
}

static const struct dev_pm_ops hdmi_pm_ops = {
	.runtime_suspend = hdmi_runtime_suspend,
	.runtime_resume = hdmi_runtime_resume,
	.suspend = hdmi_suspend,
	.resume = hdmi_resume,
};

static struct platform_driver omapdss_hdmihw_driver = {
	.remove         = __exit_p(omapdss_hdmihw_remove),
	.driver         = {
		.name   = "omapdss_hdmi",
		.owner  = THIS_MODULE,
		.pm	= &hdmi_pm_ops,
	},
};

int __init hdmi_init_platform_driver(void)
{
	return platform_driver_probe(&omapdss_hdmihw_driver, omapdss_hdmihw_probe);
}

void __exit hdmi_uninit_platform_driver(void)
{
	platform_driver_unregister(&omapdss_hdmihw_driver);
}
