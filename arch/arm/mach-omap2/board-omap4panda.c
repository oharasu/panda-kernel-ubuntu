/*
 * Board support file for OMAP4430 based PandaBoard.
 *
 * Copyright (C) 2010 Texas Instruments
 *
 * Author: David Anders <x0132446@ti.com>
 *
 * Based on mach-omap2/board-4430sdp.c
 *
 * Author: Santosh Shilimkar <santosh.shilimkar@ti.com>
 *
 * Based on mach-omap2/board-3430sdp.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/leds.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/usb/otg.h>
#include <linux/hwspinlock.h>
#include <linux/i2c/twl.h>
#include <linux/mfd/twl6040.h>
#include <linux/mfd/twl6040.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/wl12xx.h>
#include <linux/platform_data/omap-abe-twl6040.h>
#include <linux/ti_wilink_st.h>

#include <mach/hardware.h>
#include <asm/hardware/gic.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <video/omapdss.h>

#include <plat/board.h>
#include <plat/dma-44xx.h>
#include <plat/rpmsg_resmgr.h>
#include "common.h"
#include <plat/usb.h>
#include <plat/mmc.h>
#include <plat/remoteproc.h>
#include <video/omap-panel-tfp410.h>

#include "hsmmc.h"
#include "control.h"
#include "mux.h"
#include "common-board-devices.h"
#include "pm.h"

#define GPIO_HUB_POWER		1
#define GPIO_HUB_NRESET		62
#define GPIO_WIFI_PMENA		43
#define GPIO_WIFI_IRQ		53
#define GPIO_AUDPWRON		127
#define HDMI_GPIO_CT_CP_HPD 60 /* HPD mode enable/disable */
#define HDMI_GPIO_LS_OE 41 /* Level shifter for HDMI */
#define HDMI_GPIO_HPD  63 /* Hotplug detect */
#define TPS62361_GPIO   7
#define DVI_GPIO_MSEN 122

/* Display DVI */
#define PANDA_DVI_TFP410_POWER_DOWN_GPIO        0

/* LEDS */
#define GPIO_PANDA_LED1		7
#define GPIO_PANDA_LED2		8
#define GPIO_PANDAES_LED1	110
#define GPIO_PANDAES_LED2	8

static int
panda_kim_suspend(struct platform_device *pdev, pm_message_t msg)
{
	return 0;
}

static int
panda_kim_resume(struct platform_device *pdev)
{
	return 0;
}

struct ti_st_plat_data panda_bt_platform_data = {
        .nshutdown_gpio = 46,
        .dev_name = "/dev/ttyO1",
        .flow_cntrl = 1,
        .baud_rate = 3000000,
        .suspend = panda_kim_suspend,
        .resume = panda_kim_resume,

};

static struct platform_device wl1271_device = {
	.name	= "kim",
	.id	= -1,
	.dev	= {
		.platform_data	= &panda_bt_platform_data,
	},
};

static struct gpio_led gpio_leds[] = {
	{
		.name			= "pandaboard::status1",
		.default_trigger	= "heartbeat",
		.gpio			= 7,
	},
	{
		.name			= "pandaboard::status2",
		.default_trigger	= "mmc0",
		.gpio			= 8,
	},
};

static struct gpio_led_platform_data gpio_led_info = {
	.leds		= gpio_leds,
	.num_leds	= ARRAY_SIZE(gpio_leds),
};

static struct platform_device leds_gpio = {
	.name	= "leds-gpio",
	.id	= -1,
	.dev	= {
		.platform_data	= &gpio_led_info,
	},
};

static struct gpio_keys_button gpio_buttons[] = {
	{
		.code                   = BTN_EXTRA,
		.gpio                   = 121,
		.desc                   = "user",
		.wakeup                 = 1,
	},
};

static struct gpio_keys_platform_data gpio_key_info = {
	.buttons        = gpio_buttons,
	.nbuttons       = ARRAY_SIZE(gpio_buttons),
};

static struct gpio_keys_button gpio_buttons_4460[] = {
	{
		.code                   = BTN_EXTRA,
		.gpio                   = 113,
		.desc                   = "user",
		.wakeup                 = 1,
	},
};

static struct gpio_keys_platform_data gpio_key_info_4460 = {
	.buttons        = gpio_buttons_4460,
	.nbuttons       = ARRAY_SIZE(gpio_buttons_4460),
};

static struct platform_device keys_gpio = {
	.name   = "gpio-keys",
	.id     = -1,
	.dev    = {
		.platform_data  = &gpio_key_info,
	},
};

static struct omap_abe_twl6040_data panda_abe_audio_data = {
	/* Audio out */
	.has_hs		= ABE_TWL6040_LEFT | ABE_TWL6040_RIGHT,
	/* HandsFree through expasion connector */
	.has_hf		= ABE_TWL6040_LEFT | ABE_TWL6040_RIGHT,
	/* PandaBoard: FM TX, PandaBoardES: can be connected to audio out */
	.has_aux	= ABE_TWL6040_LEFT | ABE_TWL6040_RIGHT,
	/* PandaBoard: FM RX, PandaBoardES: audio in */
	.has_afm	= ABE_TWL6040_LEFT | ABE_TWL6040_RIGHT,
	.has_abe	= 1,
	/* No jack detection. */
	.jack_detection	= 0,
	/* MCLK input is 38.4MHz */
	.mclk_freq	= 38400000,

};

static struct platform_device panda_abe_audio = {
	.name		= "omap-abe-twl6040",
	.id		= -1,
	.dev = {
		.platform_data = &panda_abe_audio_data,
	},
};

static struct platform_device panda_hdmi_audio_codec = {
	.name	= "hdmi-audio-codec",
	.id	= -1,
};


static struct platform_device btwilink_device = {
	.name	= "btwilink",
	.id	= -1,
};

static struct platform_device *panda_devices[] __initdata = {
	&leds_gpio,
	&keys_gpio,
	&wl1271_device,
	&panda_abe_audio,
	&panda_hdmi_audio_codec,
	&btwilink_device,
};

static const struct usbhs_omap_board_data usbhs_bdata __initconst = {
	.port_mode[0] = OMAP_EHCI_PORT_MODE_PHY,
	.port_mode[1] = OMAP_USBHS_PORT_MODE_UNUSED,
	.port_mode[2] = OMAP_USBHS_PORT_MODE_UNUSED,
	.phy_reset  = false,
	.reset_gpio_port[0]  = -EINVAL,
	.reset_gpio_port[1]  = -EINVAL,
	.reset_gpio_port[2]  = -EINVAL,
	.clock_name = "auxclk3_ck",
	.clock_rate = 19200000,
};

/*
 * hub_nreset also enables the ULPI PHY
 * 	ULPI PHY is always powered
 * hub_power enables a 3.3V regulator for (hub + eth) chip
 *	however there's no point having ULPI PHY in use alone
 *	since it's only connected to the (hub + eth) chip
 */

static struct regulator_init_data panda_hub = {
	.constraints = {
		.name = "vhub",
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
};

static struct fixed_voltage_config panda_vhub = {
	.supply_name = "vhub",
	.microvolts = 3300000,
	.gpio = GPIO_HUB_POWER,
	.startup_delay = 70000, /* 70msec */
	.enable_high = 1,
	.enabled_at_boot = 0,
	.init_data = &panda_hub,
};

static struct platform_device omap_vhub_device = {
	.name		= "reg-fixed-voltage",
	.id		= 2,
	.dev = {
		.platform_data = &panda_vhub,
	},
};

static struct regulator_init_data panda_ulpireset = {
	/*
	 * idea is that when operating ulpireset, regulator api will make
	 * sure that the hub+eth chip is powered, since it's the "parent"
	 */
	.supply_regulator = "vhub", /* we are a child of vhub */
	.constraints = {
		.name = "hsusb0",
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
};

static struct fixed_voltage_config panda_vulpireset = {
	.supply_name = "hsusb0",  /* this name is magic for hsusb driver */
	.microvolts = 3300000,
	.gpio = GPIO_HUB_NRESET,
	.startup_delay = 70000, /* 70msec */
	.enable_high = 1,
	.enabled_at_boot = 0,
	.init_data = &panda_ulpireset,
};

static struct platform_device omap_vulpireset_device = {
	.name		= "reg-fixed-voltage",
	.id		= 3,
	.dev = {
		.platform_data = &panda_vulpireset,
	},
};

static struct omap_musb_board_data musb_board_data = {
	.interface_type		= MUSB_INTERFACE_UTMI,
	.mode			= MUSB_OTG,
	.power			= 100,
};

static struct omap2_hsmmc_info mmc[] = {
	{
		.mmc		= 1,
		.caps		= MMC_CAP_4_BIT_DATA | MMC_CAP_8_BIT_DATA,
		.gpio_wp	= -EINVAL,
		.gpio_cd	= -EINVAL,
	},
	{
		.name		= "wl1271",
		.mmc		= 5,
		.caps		= MMC_CAP_4_BIT_DATA | MMC_CAP_POWER_OFF_CARD,
		.pm_caps	= MMC_PM_KEEP_POWER,
		.gpio_wp	= -EINVAL,
		.gpio_cd	= -EINVAL,
		.ocr_mask	= MMC_VDD_165_195,
		.nonremovable	= true,
	},
	{}	/* Terminator */
};

static struct regulator_consumer_supply omap4_panda_vmmc5_supply[] = {
	REGULATOR_SUPPLY("vmmc", "omap_hsmmc.4"),
};

static struct regulator_init_data panda_vmmc5 = {
	.constraints = {
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies = ARRAY_SIZE(omap4_panda_vmmc5_supply),
	.consumer_supplies = omap4_panda_vmmc5_supply,
};

static struct fixed_voltage_config panda_vwlan = {
	.supply_name = "vwl1271",
	.microvolts = 1800000, /* 1.8V */
	.gpio = GPIO_WIFI_PMENA,
	.startup_delay = 70000, /* 70msec */
	.enable_high = 1,
	.enabled_at_boot = 0,
	.init_data = &panda_vmmc5,
};

static struct platform_device omap_vwlan_device = {
	.name		= "reg-fixed-voltage",
	.id		= 1,
	.dev = {
		.platform_data = &panda_vwlan,
	},
};

struct wl12xx_platform_data omap_panda_wlan_data  __initdata = {
	/* PANDA ref clock is 38.4 MHz */
	.board_ref_clock = 2,
	.pwr_in_suspend = true,
};

static struct omap_rprm_regulator sdp4430_rprm_regulators[] = {
	{
		.name = "cam2pwr",
	},
};

static struct regulator_consumer_supply sdp4430_cam2_supply[] = {
	{
		.supply = "cam2pwr",
	},
};

static struct regulator_init_data sdp4430_vaux3 = {
	.constraints = {
		.min_uV                 = 1000000,
		.max_uV                 = 3000000,
		.apply_uV               = true,
		.valid_modes_mask       = REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask  = REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = sdp4430_cam2_supply,
};

static int omap4_twl6030_hsmmc_late_init(struct device *dev)
{
	int irq = 0;
	struct platform_device *pdev = container_of(dev,
				struct platform_device, dev);
	struct omap_mmc_platform_data *pdata = dev->platform_data;

	if (!pdata) {
		dev_err(dev, "%s: NULL platform data\n", __func__);
		return -EINVAL;
	}
	/* Setting MMC1 Card detect Irq */
	if (pdev->id == 0) {
		irq = twl6030_mmc_card_detect_config();
		if (irq < 0) {
			dev_err(dev, "%s: Error card detect config(%d)\n",
				__func__, irq);
			return irq;
		}
		pdata->slots[0].card_detect = twl6030_mmc_card_detect;
	}
	return 0;
}

static __init void omap4_twl6030_hsmmc_set_late_init(struct device *dev)
{
	struct omap_mmc_platform_data *pdata;

	/* dev can be null if CONFIG_MMC_OMAP_HS is not set */
	if (!dev) {
		pr_err("Failed omap4_twl6030_hsmmc_set_late_init\n");
		return;
	}
	pdata = dev->platform_data;

	pdata->init =	omap4_twl6030_hsmmc_late_init;
}

static int __init omap4_twl6030_hsmmc_init(struct omap2_hsmmc_info *controllers)
{
	struct omap2_hsmmc_info *c;

	omap_hsmmc_init(controllers);
	for (c = controllers; c->mmc; c++)
		omap4_twl6030_hsmmc_set_late_init(&c->pdev->dev);

	return 0;
}

static struct twl6040_codec_data twl6040_codec = {
	/* single-step ramp for headset and handsfree */
	.hs_left_step	= 0x0f,
	.hs_right_step	= 0x0f,
	.hf_left_step	= 0x1d,
	.hf_right_step	= 0x1d,
};

static struct twl6040_platform_data twl6040_data = {
	.codec		= &twl6040_codec,
	.audpwron_gpio	= GPIO_AUDPWRON,
	.irq_base	= TWL6040_CODEC_IRQ_BASE,
};

/* Panda board uses the common PMIC configuration */
static struct twl4030_platform_data omap4_panda_twldata = {
	/* Regulators */
	.vaux3		= &sdp4430_vaux3,
};

static struct omap_i2c_bus_board_data __initdata panda_i2c_1_bus_pdata;
static struct omap_i2c_bus_board_data __initdata panda_i2c_2_bus_pdata;
static struct omap_i2c_bus_board_data __initdata panda_i2c_3_bus_pdata;
static struct omap_i2c_bus_board_data __initdata panda_i2c_4_bus_pdata;

/*
 * Display monitor features are burnt in their EEPROM as EDID data. The EEPROM
 * is connected as I2C slave device, and can be accessed at address 0x50
 */
static struct i2c_board_info __initdata panda_i2c_eeprom[] = {
	{
		I2C_BOARD_INFO("eeprom", 0x50),
	},
};

static void __init omap_i2c_hwspinlock_init(int bus_id, int spinlock_id,
					struct omap_i2c_bus_board_data *pdata)
{
	/* spinlock_id should be -1 for a generic lock request */
	if (spinlock_id < 0)
		pdata->handle = hwspin_lock_request(USE_MUTEX_LOCK);
	else
		pdata->handle = hwspin_lock_request_specific(spinlock_id, USE_MUTEX_LOCK);

	if (pdata->handle != NULL) {
		pdata->hwspin_lock_timeout = hwspin_lock_timeout;
		pdata->hwspin_unlock = hwspin_unlock;
	} else {
		pr_err("I2C hwspinlock request failed for bus %d\n", bus_id);
	}
}

static int __init omap4_panda_i2c_init(void)
{
	omap_i2c_hwspinlock_init(1, 0, &panda_i2c_1_bus_pdata);
	omap_i2c_hwspinlock_init(2, 1, &panda_i2c_2_bus_pdata);
	omap_i2c_hwspinlock_init(3, 2, &panda_i2c_3_bus_pdata);
	omap_i2c_hwspinlock_init(4, 3, &panda_i2c_4_bus_pdata);

	omap_register_i2c_bus_board_data(1, &panda_i2c_1_bus_pdata);
	omap_register_i2c_bus_board_data(2, &panda_i2c_2_bus_pdata);
	omap_register_i2c_bus_board_data(3, &panda_i2c_3_bus_pdata);
	omap_register_i2c_bus_board_data(4, &panda_i2c_4_bus_pdata);

	omap4_pmic_get_config(&omap4_panda_twldata, TWL_COMMON_PDATA_USB,
			TWL_COMMON_REGULATOR_VDAC |
			TWL_COMMON_REGULATOR_VAUX2 |
			TWL_COMMON_REGULATOR_VMMC |
			TWL_COMMON_REGULATOR_VPP |
			TWL_COMMON_REGULATOR_VANA |
			TWL_COMMON_REGULATOR_VCXIO |
			TWL_COMMON_REGULATOR_VUSB |
			TWL_COMMON_REGULATOR_CLK32KG |
			TWL_COMMON_REGULATOR_V1V8 |
			TWL_COMMON_REGULATOR_V2V1);
	omap4_pmic_init("twl6030", &omap4_panda_twldata,
			&twl6040_data, OMAP44XX_IRQ_SYS_2N);
	omap_register_i2c_bus(2, 400, NULL, 0);
	/*
	 * Bus 3 is attached to the DVI port where devices like the pico DLP
	 * projector don't work reliably with 400kHz
	 */
	omap_register_i2c_bus(3, 100, panda_i2c_eeprom,
					ARRAY_SIZE(panda_i2c_eeprom));
	omap_register_i2c_bus(4, 400, NULL, 0);
	return 0;
}

#ifdef CONFIG_OMAP_MUX
static struct omap_board_mux board_mux[] __initdata = {
	/* WLAN IRQ - GPIO 53 */
	OMAP4_MUX(GPMC_NCS3, OMAP_MUX_MODE3 | OMAP_PIN_INPUT),
	/* WLAN POWER ENABLE - GPIO 43 */
	OMAP4_MUX(GPMC_A19, OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT),
	/* WLAN SDIO: MMC5 CMD */
	OMAP4_MUX(SDMMC5_CMD, OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP),
	/* WLAN SDIO: MMC5 CLK */
	OMAP4_MUX(SDMMC5_CLK, OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP),
	/* WLAN SDIO: MMC5 DAT[0-3] */
	OMAP4_MUX(SDMMC5_DAT0, OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP),
	OMAP4_MUX(SDMMC5_DAT1, OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP),
	OMAP4_MUX(SDMMC5_DAT2, OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP),
	OMAP4_MUX(SDMMC5_DAT3, OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP),
	/* gpio 0 - TFP410 PD */
	OMAP4_MUX(KPD_COL1, OMAP_PIN_OUTPUT | OMAP_MUX_MODE3 | OMAP_PIN_OFF_OUTPUT_LOW),
	/* GPIO 122 - TFP410 MSEN input */
	OMAP4_MUX(ABE_DMIC_DIN3, OMAP_PIN_INPUT | OMAP_MUX_MODE3 | OMAP_PIN_OFF_INPUT_PULLDOWN),
	/* dispc2_data23 */
	OMAP4_MUX(USBB2_ULPITLL_STP, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data22 */
	OMAP4_MUX(USBB2_ULPITLL_DIR, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data21 */
	OMAP4_MUX(USBB2_ULPITLL_NXT, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data20 */
	OMAP4_MUX(USBB2_ULPITLL_DAT0, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data19 */
	OMAP4_MUX(USBB2_ULPITLL_DAT1, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data18 */
	OMAP4_MUX(USBB2_ULPITLL_DAT2, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data15 */
	OMAP4_MUX(USBB2_ULPITLL_DAT3, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data14 */
	OMAP4_MUX(USBB2_ULPITLL_DAT4, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data13 */
	OMAP4_MUX(USBB2_ULPITLL_DAT5, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data12 */
	OMAP4_MUX(USBB2_ULPITLL_DAT6, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data11 */
	OMAP4_MUX(USBB2_ULPITLL_DAT7, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data10 */
	OMAP4_MUX(DPM_EMU3, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data9 */
	OMAP4_MUX(DPM_EMU4, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data16 */
	OMAP4_MUX(DPM_EMU5, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data17 */
	OMAP4_MUX(DPM_EMU6, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_hsync */
	OMAP4_MUX(DPM_EMU7, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_pclk */
	OMAP4_MUX(DPM_EMU8, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_vsync */
	OMAP4_MUX(DPM_EMU9, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_de */
	OMAP4_MUX(DPM_EMU10, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data8 */
	OMAP4_MUX(DPM_EMU11, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data7 */
	OMAP4_MUX(DPM_EMU12, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data6 */
	OMAP4_MUX(DPM_EMU13, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data5 */
	OMAP4_MUX(DPM_EMU14, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data4 */
	OMAP4_MUX(DPM_EMU15, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data3 */
	OMAP4_MUX(DPM_EMU16, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data2 */
	OMAP4_MUX(DPM_EMU17, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data1 */
	OMAP4_MUX(DPM_EMU18, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* dispc2_data0 */
	OMAP4_MUX(DPM_EMU19, OMAP_PIN_OUTPUT | OMAP_MUX_MODE5),
	/* AUDPWRON 127 */
	OMAP4_MUX(HDQ_SIO, OMAP_PIN_INPUT | OMAP_MUX_MODE3),
	/* ABE Clock */
	OMAP4_MUX(ABE_CLKS, OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLDOWN
		| OMAP_PIN_OFF_INPUT_PULLDOWN | OMAP_PIN_OFF_OUTPUT_LOW),

	{ .reg_offset = OMAP_MUX_TERMINATOR },
};

#else
#define board_mux	NULL
#endif


/* Using generic display panel */
static struct tfp410_platform_data omap4_dvi_panel = {
	.i2c_bus_num		= 3,
	.power_down_gpio	= PANDA_DVI_TFP410_POWER_DOWN_GPIO,
};

struct omap_dss_device omap4_panda_dvi_device = {
	.type			= OMAP_DISPLAY_TYPE_DPI,
	.name			= "dvi",
	.driver_name		= "tfp410",
	.data			= &omap4_dvi_panel,
	.phy.dpi.data_lines	= 24,
	.reset_gpio		= PANDA_DVI_TFP410_POWER_DOWN_GPIO,
	.channel		= OMAP_DSS_CHANNEL_LCD2,
	.clocks.fck_div		= 9,
	.clocks.dispc.channel.lcd_clk_src = OMAP_DSS_CLK_SRC_DSI2_PLL_HSDIV_DISPC,
	.clocks.dispc.dispc_fclk_src = OMAP_DSS_CLK_SRC_DSI2_PLL_HSDIV_DISPC,
};

static struct gpio panda_dvi_gpios_phy[] = {
	{ DVI_GPIO_MSEN,      GPIOF_DIR_IN, "dvi_msen" },
};

static struct omap_dss_hdmi_data omap4_panda_hdmi_data = {
	.ct_cp_hpd_gpio = HDMI_GPIO_CT_CP_HPD,
	.ls_oe_gpio = HDMI_GPIO_LS_OE,
	.hpd_gpio = HDMI_GPIO_HPD,
};

static struct omap_dss_device  omap4_panda_hdmi_device = {
	.name = "hdmi",
	.driver_name = "hdmi_panel",
	.type = OMAP_DISPLAY_TYPE_HDMI,
	.channel = OMAP_DSS_CHANNEL_DIGIT,
	.data = &omap4_panda_hdmi_data,
	.clocks.fck_div = 9,
};

static struct omap_dss_device *omap4_panda_dss_devices[] = {
	&omap4_panda_dvi_device,
	&omap4_panda_hdmi_device,
};

static struct omap_dss_board_info omap4_panda_dss_data = {
	.num_devices	= ARRAY_SIZE(omap4_panda_dss_devices),
	.devices	= omap4_panda_dss_devices,
	.default_device	= &omap4_panda_dvi_device,
};

/*
 * These device paths represent the onboard USB <-> Ethernet bridge, and
 * the WLAN module on Panda, both of which need their random or all-zeros
 * mac address replacing with a per-cpu stable generated one
 */
static const char * const panda_fixup_mac_device_paths[] = {
       "usb1/1-1/1-1.1/1-1.1:1.0",
       "wl12xx",
};

void __init omap4_panda_display_init(void)
{
	int status;

	status = gpio_request_array(panda_dvi_gpios_phy,
                                    ARRAY_SIZE(panda_dvi_gpios_phy));

	omap_display_init(&omap4_panda_dss_data);

	/*
	 * OMAP4460SDP/Blaze and OMAP4430 ES2.3 SDP/Blaze boards and
	 * later have external pull up on the HDMI I2C lines
	 */
	if (cpu_is_omap446x() || omap_rev() > OMAP4430_REV_ES2_2)
		omap_hdmi_init(OMAP_HDMI_SDA_SCL_EXTERNAL_PULLUP);
	else
		omap_hdmi_init(0);

	omap_mux_init_gpio(HDMI_GPIO_LS_OE, OMAP_PIN_OUTPUT);
	omap_mux_init_gpio(HDMI_GPIO_CT_CP_HPD, OMAP_PIN_OUTPUT);
	omap_mux_init_gpio(HDMI_GPIO_HPD, OMAP_PIN_INPUT_PULLDOWN);
}

static void omap4_panda_init_rev(void)
{
	if (cpu_is_omap443x()) {
		/* PandaBoard 4430 */
		/* ASoC audio configuration */
		panda_abe_audio_data.card_name = "Panda";
		panda_abe_audio_data.has_hsmic = 1;
	} else {
		/* PandaBoard ES */
		/* ASoC audio configuration */
		panda_abe_audio_data.card_name = "PandaES";
	}
}

static void __init enable_board_wakeups(void)
{
	/* user button on GPIO_121 for 4430, 113 for 4460 */
	if (cpu_is_omap446x())
		omap_mux_init_signal("gpio_113",
			OMAP_WAKEUP_EN | OMAP_PIN_INPUT_PULLUP);
	else
		omap_mux_init_signal("gpio_121",
			OMAP_WAKEUP_EN | OMAP_PIN_INPUT_PULLUP);

	/* sys_nirq1 for TWL6030 (USB, PMIC, etc) */
	omap_mux_init_signal("sys_nirq1",
		OMAP_WAKEUP_EN | OMAP_PIN_INPUT_PULLUP);

	/* wake on WLAN */
	omap_mux_init_gpio(GPIO_WIFI_IRQ, OMAP_PIN_INPUT |
			OMAP_PIN_OFF_WAKEUPENABLE);

}

static struct gpio panda_gpio_non_configured_init[] = {
#ifndef CONFIG_PANEL_TFP410
	{ PANDA_DVI_TFP410_POWER_DOWN_GPIO,
				GPIOF_OUT_INIT_LOW, "TFP410 NPD" },
#endif
#ifndef CONFIG_LEDS_GPIO
	{ GPIO_PANDA_LED1,	GPIOF_OUT_INIT_LOW, "PANDA LED 1" },
	{ GPIO_PANDA_LED2,	GPIOF_OUT_INIT_LOW, "PANDA LED 2" },
#endif
};

static struct gpio pandaes_gpio_non_configured_init[] = {
#ifndef CONFIG_PANEL_TFP410
	{ PANDA_DVI_TFP410_POWER_DOWN_GPIO,
				GPIOF_OUT_INIT_LOW, "TFP410 NPD" },
#endif
#ifndef CONFIG_LEDS_GPIO
	{ GPIO_PANDAES_LED1,	GPIOF_OUT_INIT_LOW, "PANDA-ES LED 1" },
	{ GPIO_PANDAES_LED2,	GPIOF_OUT_INIT_LOW, "PANDA-ES LED 2" },
#endif
};

void panda_init_non_configured_gpio(void)
{
	/* Setup non configured GPIOs if any, but release them to allow
	 * another user to use them later. It is expected that GPIOs
	 * some attributes will be preserved once freed (direction and 
	 * value).
	 */
	if (cpu_is_omap443x()) {
		gpio_request_array(panda_gpio_non_configured_init,
			    ARRAY_SIZE(panda_gpio_non_configured_init));
		gpio_free_array(panda_gpio_non_configured_init,
			    ARRAY_SIZE(panda_gpio_non_configured_init));
	}
	if (cpu_is_omap446x()) {
		gpio_request_array(pandaes_gpio_non_configured_init,
			    ARRAY_SIZE(pandaes_gpio_non_configured_init));
		gpio_free_array(pandaes_gpio_non_configured_init,
			    ARRAY_SIZE(pandaes_gpio_non_configured_init));
	}
}

static void __init omap4_panda_init(void)
{
	int package = OMAP_PACKAGE_CBS;
	int ret;

	if (omap_rev() == OMAP4430_REV_ES1_0)
		package = OMAP_PACKAGE_CBL;

	omap_emif_set_device_details(1, &lpddr2_elpida_2G_S4_x2_info,
			lpddr2_elpida_2G_S4_timings,
			ARRAY_SIZE(lpddr2_elpida_2G_S4_timings),
			&lpddr2_elpida_S4_min_tck, NULL);

	omap_emif_set_device_details(2, &lpddr2_elpida_2G_S4_x2_info,
			lpddr2_elpida_2G_S4_timings,
			ARRAY_SIZE(lpddr2_elpida_2G_S4_timings),
			&lpddr2_elpida_S4_min_tck, NULL);

	if (cpu_is_omap446x()) {
		gpio_leds[0].gpio = GPIO_PANDAES_LED1;
		keys_gpio.dev.platform_data = &gpio_key_info_4460;
	}

	omap4_mux_init(board_mux, NULL, package);

	omap_panda_wlan_data.irq = gpio_to_irq(GPIO_WIFI_IRQ);

	omap_register_mac_device_fixup_paths(panda_fixup_mac_device_paths,
				     ARRAY_SIZE(panda_fixup_mac_device_paths));

	ret = wl12xx_set_platform_data(&omap_panda_wlan_data);
	if (ret)
		pr_err("error setting wl12xx data: %d\n", ret);

	omap4_panda_init_rev();
	omap4_panda_i2c_init();
	panda_init_non_configured_gpio();
	platform_add_devices(panda_devices, ARRAY_SIZE(panda_devices));
	platform_device_register(&omap_vwlan_device);
	platform_device_register(&omap_vhub_device);
	platform_device_register(&omap_vulpireset_device);
	omap_serial_init();
	omap_sdrc_init(NULL, NULL);
	omap4_twl6030_hsmmc_init(mmc);
	usbhs_init(&usbhs_bdata);
	usb_musb_init(&musb_board_data);
	omap4_panda_display_init();
	if (cpu_is_omap446x()) {
		/* Vsel0 = gpio, vsel1 = gnd */
		ret = omap_tps6236x_board_setup(true, TPS62361_GPIO, -1,
				OMAP_PIN_OFF_OUTPUT_HIGH, -1);
		if (ret)
			pr_err("TPS62361 initialization failed: %d\n", ret);
	}
	omap_enable_smartreflex_on_init();
	omap_rprm_regulator_init(sdp4430_rprm_regulators,
				 ARRAY_SIZE(sdp4430_rprm_regulators));
	enable_board_wakeups();
}

static void __init omap4_panda_map_io(void)
{
        omap2_set_globals_443x();
        omap44xx_map_common_io();
}

static const char *omap4_panda_match[] = {
	"ti,omap4-panda",
	NULL,
};
static void __init omap4_panda_reserve(void)
{
	omap_rproc_reserve_cma(RPROC_CMA_OMAP4);
	omap_reserve();
}

MACHINE_START(OMAP4_PANDA, "OMAP4 Panda board")
	/* Maintainer: David Anders - Texas Instruments Inc */
	.atag_offset	= 0x100,
	.reserve	= omap4_panda_reserve,
	.map_io		= omap4_panda_map_io,
	.init_early	= omap4430_init_early,
	.init_irq	= gic_init_irq,
	.handle_irq	= gic_handle_irq,
	.init_machine	= omap4_panda_init,
	.timer		= &omap4_timer,
	.restart	= omap_prcm_restart,
	.dt_compat      = omap4_panda_match,
MACHINE_END
