/*
 * linux/arch/arm/mach-omap2/devices.c
 *
 * OMAP2 platform device setup/initialization
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/export.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/platform_data/omap4-keypad.h>
#include <linux/netdevice.h>
#include <linux/if_ether.h>
#include <linux/pm_runtime.h>
#include <media/omap3isp.h>

#include <mach/hardware.h>
#include <mach/irqs.h>
#include <asm/mach-types.h>
#include <asm/mach/map.h>
#include <asm/pmu.h>
#include <asm/cti.h>
#include <mach/id.h>

#include "iomap.h"
#include <plat/board.h>
#include <plat/mmc.h>
#include <plat/dma.h>
#include <plat/gpu.h>
#include <plat/omap_hwmod.h>
#include <plat/omap_device.h>
#include <plat/omap4-keypad.h>
#include <plat/rpmsg_resmgr.h>
#include <linux/mfd/omap_control.h>

#if defined(CONFIG_SND_OMAP_SOC_ABE_DSP) || \
	defined(CONFIG_SND_OMAP_SOC_ABE_DSP_MODULE)
#include <sound/omap-abe-dsp.h>
#endif
#include "mux.h"
#include "control.h"
#include "devices.h"

#if defined(CONFIG_SATA_AHCI_PLATFORM) || \
	defined(CONFIG_SATA_AHCI_PLATFORM_MODULE)
#include <plat/sata.h>
#endif

#define L3_MODULES_MAX_LEN 12
#define L3_MODULES 3

static const char * const *mac_device_fixup_paths;
int count_mac_device_fixup_paths;

/* macro for building platform_device for McBSP ports */
#define OMAP_MCBSP_PLATFORM_DEVICE(port_nr)             \
static struct platform_device omap_mcbsp##port_nr = {   \
        .name   = "omap-mcbsp-dai",                     \
        .id     = port_nr - 1,                  \
}

OMAP_MCBSP_PLATFORM_DEVICE(1);
OMAP_MCBSP_PLATFORM_DEVICE(2);
OMAP_MCBSP_PLATFORM_DEVICE(3);
OMAP_MCBSP_PLATFORM_DEVICE(4);
OMAP_MCBSP_PLATFORM_DEVICE(5);

static int __init omap_init_control(void)
{
	struct omap_hwmod		*oh;
	struct platform_device		*pdev;
	const char			*oh_name, *name;

	oh_name = "ctrl_module_core";
	name = "omap-control-core";

	oh = omap_hwmod_lookup(oh_name);
	if (!oh) {
		pr_err("Could not lookup hwmod for %s\n", oh_name);
		return PTR_ERR(oh);
	}

	pdev = omap_device_build(name, -1, oh, NULL, 0, NULL, 0, true);
	if (IS_ERR(pdev)) {
		pr_err("Could not build omap_device for %s %s\n",
		       name, oh_name);
		return PTR_ERR(pdev);
	}

	return 0;
}
postcore_initcall(omap_init_control);

static int __init omap3_l3_init(void)
{
	int l;
	struct omap_hwmod *oh;
	struct platform_device *pdev;
	char oh_name[L3_MODULES_MAX_LEN];

	/*
	 * To avoid code running on other OMAPs in
	 * multi-omap builds
	 */
	if (!(cpu_is_omap34xx()))
		return -ENODEV;

	l = snprintf(oh_name, L3_MODULES_MAX_LEN, "l3_main");

	oh = omap_hwmod_lookup(oh_name);

	if (!oh)
		pr_err("could not look up %s\n", oh_name);

	pdev = omap_device_build("omap_l3_smx", 0, oh, NULL, 0,
							   NULL, 0, 0);

	WARN(IS_ERR(pdev), "could not build omap_device for %s\n", oh_name);

	return IS_ERR(pdev) ? PTR_ERR(pdev) : 0;
}
postcore_initcall(omap3_l3_init);

static int __init omap4_l3_init(void)
{
	int l, i;
	struct omap_hwmod *oh[3];
	struct platform_device *pdev;
	char oh_name[L3_MODULES_MAX_LEN];

	/* If dtb is there, the devices will be created dynamically */
	if (of_have_populated_dt())
		return -ENODEV;

	/*
	 * To avoid code running on other OMAPs in
	 * multi-omap builds
	 */

	if ((!(cpu_is_omap44xx())) && (!cpu_is_omap54xx()))
		return -ENODEV;

	for (i = 0; i < L3_MODULES; i++) {
		l = snprintf(oh_name, L3_MODULES_MAX_LEN, "l3_main_%d", i+1);

		oh[i] = omap_hwmod_lookup(oh_name);
		if (!(oh[i]))
			pr_err("could not look up %s\n", oh_name);
	}

	pdev = omap_device_build_ss("omap_l3_noc", 0, oh, 3, NULL,
						     0, NULL, 0, 0);

	WARN(IS_ERR(pdev), "could not build omap_device for %s\n", oh_name);

	return IS_ERR(pdev) ? PTR_ERR(pdev) : 0;
}
postcore_initcall(omap4_l3_init);

#if defined(CONFIG_VIDEO_OMAP2) || defined(CONFIG_VIDEO_OMAP2_MODULE)

static struct resource omap2cam_resources[] = {
	{
		.start		= OMAP24XX_CAMERA_BASE,
		.end		= OMAP24XX_CAMERA_BASE + 0xfff,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= INT_24XX_CAM_IRQ,
		.flags		= IORESOURCE_IRQ,
	}
};

static struct platform_device omap2cam_device = {
	.name		= "omap24xxcam",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(omap2cam_resources),
	.resource	= omap2cam_resources,
};
#endif

static struct isp_platform_data bogus_isp_pdata;

#if defined(CONFIG_IOMMU_API)

#include <plat/iommu.h>

static struct resource omap3isp_resources[] = {
	{
		.start		= OMAP3430_ISP_BASE,
		.end		= OMAP3430_ISP_END,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP3430_ISP_CCP2_BASE,
		.end		= OMAP3430_ISP_CCP2_END,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP3430_ISP_CCDC_BASE,
		.end		= OMAP3430_ISP_CCDC_END,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP3430_ISP_HIST_BASE,
		.end		= OMAP3430_ISP_HIST_END,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP3430_ISP_H3A_BASE,
		.end		= OMAP3430_ISP_H3A_END,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP3430_ISP_PREV_BASE,
		.end		= OMAP3430_ISP_PREV_END,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP3430_ISP_RESZ_BASE,
		.end		= OMAP3430_ISP_RESZ_END,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP3430_ISP_SBL_BASE,
		.end		= OMAP3430_ISP_SBL_END,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP3430_ISP_CSI2A_REGS1_BASE,
		.end		= OMAP3430_ISP_CSI2A_REGS1_END,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP3430_ISP_CSIPHY2_BASE,
		.end		= OMAP3430_ISP_CSIPHY2_END,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP3630_ISP_CSI2A_REGS2_BASE,
		.end		= OMAP3630_ISP_CSI2A_REGS2_END,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP3630_ISP_CSI2C_REGS1_BASE,
		.end		= OMAP3630_ISP_CSI2C_REGS1_END,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP3630_ISP_CSIPHY1_BASE,
		.end		= OMAP3630_ISP_CSIPHY1_END,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP3630_ISP_CSI2C_REGS2_BASE,
		.end		= OMAP3630_ISP_CSI2C_REGS2_END,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= INT_34XX_CAM_IRQ,
		.flags		= IORESOURCE_IRQ,
	}
};

static struct platform_device omap3isp_device = {
	.name		= "omap3isp",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(omap3isp_resources),
	.resource	= omap3isp_resources,
};

static struct omap_iommu_arch_data omap3_isp_iommu = {
	.name = "isp",
};

int omap3_init_camera(struct isp_platform_data *pdata)
{
	omap3isp_device.dev.platform_data = pdata;
	omap3isp_device.dev.archdata.iommu = &omap3_isp_iommu;

	return platform_device_register(&omap3isp_device);
}

#else /* !CONFIG_IOMMU_API */

int omap3_init_camera(struct isp_platform_data *pdata)
{
	return 0;
}

#endif

static inline void omap_init_camera(void)
{
#if defined(CONFIG_VIDEO_OMAP2) || defined(CONFIG_VIDEO_OMAP2_MODULE)
	if (cpu_is_omap24xx())
		platform_device_register(&omap2cam_device);
#endif
}

struct omap_device_pm_latency omap_keyboard_latency[] = {
	{
		.deactivate_func = omap_device_idle_hwmods,
		.activate_func   = omap_device_enable_hwmods,
		.flags = OMAP_DEVICE_LATENCY_AUTO_ADJUST,
	},
};

int __init omap4_keyboard_init(struct omap4_keypad_platform_data
			*sdp4430_keypad_data, struct omap_board_data *bdata)
{
	struct platform_device *pdev;
	struct omap_hwmod *oh;
	struct omap4_keypad_platform_data *keypad_data;
	unsigned int id = -1;
	char *oh_name = "kbd";
	char *name = "omap4-keypad";

	oh = omap_hwmod_lookup(oh_name);
	if (!oh) {
		pr_err("Could not look up %s\n", oh_name);
		return -ENODEV;
	}

	keypad_data = sdp4430_keypad_data;

	pdev = omap_device_build(name, id, oh, keypad_data,
			sizeof(struct omap4_keypad_platform_data), NULL, 0, 0);

	if (IS_ERR(pdev)) {
		WARN(1, "Can't build omap_device for %s:%s.\n",
						name, oh->name);
		return PTR_ERR(pdev);
	}
	oh->mux = omap_hwmod_mux_init(bdata->pads, bdata->pads_cnt);

	return 0;
}

#if defined(CONFIG_OMAP_MBOX_FWK) || defined(CONFIG_OMAP_MBOX_FWK_MODULE)
static inline void __init omap_init_mbox(void)
{
	struct omap_hwmod *oh;
	struct platform_device *pdev;

	oh = omap_hwmod_lookup("mailbox");
	if (!oh) {
		pr_err("%s: unable to find hwmod\n", __func__);
		return;
	}

	pdev = omap_device_build("omap-mailbox", -1, oh, NULL, 0, NULL, 0, 0);
	WARN(IS_ERR(pdev), "%s: could not build device, err %ld\n",
						__func__, PTR_ERR(pdev));
}
#else
static inline void omap_init_mbox(void) { }
#endif /* CONFIG_OMAP_MBOX_FWK */

static inline void omap_init_sti(void) {}

#if defined CONFIG_ARCH_OMAP4 || defined CONFIG_ARCH_OMAP5

static struct platform_device codec_dmic0 = {
	.name	= "dmic-codec",
	.id	= 0,
};

static struct platform_device codec_dmic1 = {
	.name	= "dmic-codec",
	.id	= 1,
};

static struct platform_device codec_dmic2 = {
	.name	= "dmic-codec",
	.id	= 2,
};

static inline void omap_init_abe(void)
{
	platform_device_register(&codec_dmic0);
	platform_device_register(&codec_dmic1);
	platform_device_register(&codec_dmic2);
}
#else
static inline void omap_init_abe(void) {}
#endif

#if defined(CONFIG_SND_SOC) || defined(CONFIG_SND_SOC_MODULE)
static struct platform_device omap_pcm = {
	.name	= "omap-pcm-audio",
	.id	= -1,
};

#if defined(CONFIG_SND_OMAP_SOC_VXREC) || defined(CONFIG_SND_OMAP_SOC_VXREC_MODULE)
static struct platform_device omap_abe_vxrec = {
	.name   = "omap-abe-vxrec-dai",
	.id     = -1,
};
#endif

static void omap_init_audio(void)
{
	platform_device_register(&omap_mcbsp1);
	platform_device_register(&omap_mcbsp2);
	if (cpu_class_is_omap2() && !cpu_is_omap242x()) {
		platform_device_register(&omap_mcbsp3);
		if (!cpu_is_omap54xx())
			platform_device_register(&omap_mcbsp4);
	}
	if (cpu_is_omap243x() || cpu_is_omap34xx())
		platform_device_register(&omap_mcbsp5);

	platform_device_register(&omap_pcm);
#if defined(CONFIG_SND_OMAP_SOC_VXREC) || defined(CONFIG_SND_OMAP_SOC_VXREC_MODULE)
	platform_device_register(&omap_abe_vxrec);
#endif
}

#else
static inline void omap_init_audio(void) {}
#endif

#if defined(CONFIG_SND_OMAP_SOC_MCASP) || \
	defined(CONFIG_SND_OMAP_SOC_MCASP_MODULE)
static struct omap_device_pm_latency omap_mcasp_latency[] = {
	{
		.deactivate_func = omap_device_idle_hwmods,
		.activate_func = omap_device_enable_hwmods,
		.flags = OMAP_DEVICE_LATENCY_AUTO_ADJUST,
	},
};

static void omap_init_mcasp(void)
{
	struct omap_hwmod *oh;
	struct platform_device *pdev;

	oh = omap_hwmod_lookup("mcasp");
	if (!oh) {
		pr_err("could not look up mcasp hw_mod\n");
		return;
	}

	pdev = omap_device_build("omap-mcasp", -1, oh, NULL, 0,
				omap_mcasp_latency,
				ARRAY_SIZE(omap_mcasp_latency), 0);
	WARN(IS_ERR(pdev), "Can't build omap_device for omap-mcasp-audio.\n");
}
#else
static inline void omap_init_mcasp(void) {}
#endif

#if defined(CONFIG_SND_OMAP_SOC_MCPDM) || \
		defined(CONFIG_SND_OMAP_SOC_MCPDM_MODULE)

static struct omap_device_pm_latency omap_mcpdm_latency[] = {
	{
		.deactivate_func = omap_device_idle_hwmods,
		.activate_func = omap_device_enable_hwmods,
		.flags = OMAP_DEVICE_LATENCY_AUTO_ADJUST,
	},
};

static void __init omap_init_mcpdm(void)
{
	struct omap_hwmod *oh;
	 struct platform_device *pdev;

	oh = omap_hwmod_lookup("mcpdm");
	if (!oh) {
		printk(KERN_ERR "Could not look up mcpdm hw_mod\n");
		return;
	}

	pdev = omap_device_build("omap-mcpdm", -1, oh, NULL, 0,
				omap_mcpdm_latency,
				ARRAY_SIZE(omap_mcpdm_latency), 0);
	if (IS_ERR(pdev))
		printk(KERN_ERR "Could not build omap_device for omap-mcpdm-dai\n");
}
#else
static inline void omap_init_mcpdm(void) {}
#endif

#if defined(CONFIG_SND_OMAP_SOC_DMIC) || defined(CONFIG_SND_SOC_DMIC) || \
		defined(CONFIG_SND_OMAP_SOC_DMIC_MODULE)

static void __init omap_init_dmic(void)
{
	struct omap_hwmod *oh;
	struct platform_device *pdev;

	oh = omap_hwmod_lookup("dmic");
	if (!oh) {
		printk(KERN_ERR "Could not look up mcpdm hw_mod\n");
		return;
	}

	pdev = omap_device_build("omap-dmic", -1, oh, NULL, 0, NULL, 0, 0);
	WARN(IS_ERR(pdev), "Can't build omap_device for omap-dmic.\n");
}
#else
static inline void omap_init_dmic(void) {}
#endif

#if defined(CONFIG_SND_OMAP_SOC_ABE_DSP) || \
	defined(CONFIG_SND_OMAP_SOC_ABE) || \
	defined(CONFIG_SND_OMAP_SOC_ABE_MODULE) || \
	defined(CONFIG_SND_OMAP_SOC_ABE_DSP_MODULE) || \
	defined(CONFIG_SND_OMAP_SOC_OMAP_ABE_TWL6040)

static struct omap_device_pm_latency omap_aess_latency[] = {
	{
		.deactivate_func = omap_device_idle_hwmods,
		.activate_func = omap_device_enable_hwmods,
		.flags = OMAP_DEVICE_LATENCY_AUTO_ADJUST,
	},
};

static void __init omap_init_aess(void)
{
	struct omap_hwmod *oh;
//	struct omap4_abe_dsp_pdata *pdata;
	struct platform_device *pdev;

	oh = omap_hwmod_lookup("aess");
	if (!oh) {
		pr_err("Could not look up aess hw_mod\n");
		return;
	}
/*
	pdata = kzalloc(sizeof(struct omap4_abe_dsp_pdata), GFP_KERNEL);
	if (!pdata) {
		pr_err("%s Could not allocate platform data\n", __func__);
		return;
	}
*/
	/* FIXME: Add correct context loss counter */
	/*pdata->get_context_loss_count = omap_pm_get_dev_context_loss_count;*/

	pdev = omap_device_build("aess", -1, oh, /* pdata */ NULL, 0,
//				sizeof(struct omap4_abe_dsp_pdata),
				omap_aess_latency,
				ARRAY_SIZE(omap_aess_latency), 0);

	//kfree(pdata);

	if (IS_ERR(pdev))
		pr_err("Could not build omap_device for omap-aess-audio\n");
}
#else
static inline void omap_init_aess(void) {}
#endif

#if defined(CONFIG_SND_OMAP_SOC_OMAP_HDMI) || \
		defined(CONFIG_SND_OMAP_SOC_OMAP_HDMI_MODULE)

static struct platform_device omap_hdmi_audio = {
	.name	= "omap-hdmi-audio",
	.id	= -1,
};

static void __init omap_init_hdmi_audio(void)
{
	struct omap_hwmod *oh;
	struct platform_device *pdev;

	oh = omap_hwmod_lookup("dss_hdmi");
	if (!oh) {
		printk(KERN_ERR "Could not look up dss_hdmi hw_mod\n");
		return;
	}

	pdev = omap_device_build("omap-hdmi-audio-dai",
		-1, oh, NULL, 0, NULL, 0, 0);
	WARN(IS_ERR(pdev),
	     "Can't build omap_device for omap-hdmi-audio-dai.\n");

	platform_device_register(&omap_hdmi_audio);
}
#else
static inline void omap_init_hdmi_audio(void) {}
#endif

#if defined(CONFIG_SPI_OMAP24XX) || defined(CONFIG_SPI_OMAP24XX_MODULE)

#include <plat/mcspi.h>

static int __init omap_mcspi_init(struct omap_hwmod *oh, void *unused)
{
	struct platform_device *pdev;
	char *name = "omap2_mcspi";
	struct omap2_mcspi_platform_config *pdata;
	static int spi_num;
	struct omap2_mcspi_dev_attr *mcspi_attrib = oh->dev_attr;

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		pr_err("Memory allocation for McSPI device failed\n");
		return -ENOMEM;
	}

	pdata->num_cs = mcspi_attrib->num_chipselect;
	switch (oh->class->rev) {
	case OMAP2_MCSPI_REV:
	case OMAP3_MCSPI_REV:
			pdata->regs_offset = 0;
			break;
	case OMAP4_MCSPI_REV:
			pdata->regs_offset = OMAP4_MCSPI_REG_OFFSET;
			break;
	default:
			pr_err("Invalid McSPI Revision value\n");
			kfree(pdata);
			return -EINVAL;
	}

	spi_num++;
	pdev = omap_device_build(name, spi_num, oh, pdata,
				sizeof(*pdata),	NULL, 0, 0);
	WARN(IS_ERR(pdev), "Can't build omap_device for %s:%s\n",
				name, oh->name);
	kfree(pdata);
	return 0;
}

static void omap_init_mcspi(void)
{
	omap_hwmod_for_each_by_class("mcspi", omap_mcspi_init, NULL);
}

#else
static inline void omap_init_mcspi(void) {}
#endif

static struct resource omap2_pmu_resource = {
	.start	= 3,
	.end	= 3,
	.flags	= IORESOURCE_IRQ,
};

static struct resource omap3_pmu_resource = {
	.start	= INT_34XX_BENCH_MPU_EMUL,
	.end	= INT_34XX_BENCH_MPU_EMUL,
	.flags	= IORESOURCE_IRQ,
};

static struct resource omap4_pmu_resource[] = {
	{
		.start	= OMAP44XX_IRQ_CTI0,
		.end	= OMAP44XX_IRQ_CTI0,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= OMAP44XX_IRQ_CTI1,
		.end	= OMAP44XX_IRQ_CTI1,
		.flags	= IORESOURCE_IRQ,
	}
};

static struct platform_device omap_pmu_device = {
	.name		= "arm-pmu",
	.id		= ARM_PMU_DEVICE_CPU,
	.num_resources	= 1,
};

static struct arm_pmu_platdata omap4_pmu_data;
static struct cti omap4_cti[2];

static void omap4_enable_cti(int irq)
{
	if (irq == OMAP44XX_IRQ_CTI0)
		cti_enable(&omap4_cti[0]);
	else if (irq == OMAP44XX_IRQ_CTI1)
		cti_enable(&omap4_cti[1]);
}

static void omap4_disable_cti(int irq)
{
	if (irq == OMAP44XX_IRQ_CTI0)
		cti_disable(&omap4_cti[0]);
	else if (irq == OMAP44XX_IRQ_CTI1)
		cti_disable(&omap4_cti[1]);
}

static irqreturn_t omap4_pmu_handler(int irq, void *dev, irq_handler_t handler)
{
	if (irq == OMAP44XX_IRQ_CTI0)
		cti_irq_ack(&omap4_cti[0]);
	else if (irq == OMAP44XX_IRQ_CTI1)
		cti_irq_ack(&omap4_cti[1]);

	return handler(irq, dev);
}

static void omap4_configure_pmu_irq(void)
{
	void __iomem *base0;
	void __iomem *base1;

	base0 = ioremap(OMAP44XX_CTI0_BASE, SZ_4K);
	base1 = ioremap(OMAP44XX_CTI1_BASE, SZ_4K);
	if (!base0 && !base1) {
		pr_err("ioremap for OMAP4 CTI failed\n");
		return;
	}

	/*configure CTI0 for pmu irq routing*/
	cti_init(&omap4_cti[0], base0, OMAP44XX_IRQ_CTI0, 6);
	cti_unlock(&omap4_cti[0]);
	cti_map_trigger(&omap4_cti[0], 1, 6, 2);

	/*configure CTI1 for pmu irq routing*/
	cti_init(&omap4_cti[1], base1, OMAP44XX_IRQ_CTI1, 6);
	cti_unlock(&omap4_cti[1]);
	cti_map_trigger(&omap4_cti[1], 1, 6, 2);

	omap4_pmu_data.handle_irq = omap4_pmu_handler;
	omap4_pmu_data.enable_irq = omap4_enable_cti;
	omap4_pmu_data.disable_irq = omap4_disable_cti;
}

static void omap_init_pmu(void)
{
	if (cpu_is_omap24xx())
		omap_pmu_device.resource = &omap2_pmu_resource;
	else if (cpu_is_omap34xx())
		omap_pmu_device.resource = &omap3_pmu_resource;
	else if (cpu_is_omap44xx()) {
		omap_pmu_device.resource = omap4_pmu_resource;
		omap_pmu_device.num_resources = 2;
		omap_pmu_device.dev.platform_data = &omap4_pmu_data;
		omap4_configure_pmu_irq();
	} else
		return;

	platform_device_register(&omap_pmu_device);
}


#if defined(CONFIG_CRYPTO_DEV_OMAP_SHAM) || defined(CONFIG_CRYPTO_DEV_OMAP_SHAM_MODULE)

#ifdef CONFIG_ARCH_OMAP2
static struct resource omap2_sham_resources[] = {
	{
		.start	= OMAP24XX_SEC_SHA1MD5_BASE,
		.end	= OMAP24XX_SEC_SHA1MD5_BASE + 0x64,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= INT_24XX_SHA1MD5,
		.flags	= IORESOURCE_IRQ,
	}
};
static int omap2_sham_resources_sz = ARRAY_SIZE(omap2_sham_resources);
#else
#define omap2_sham_resources		NULL
#define omap2_sham_resources_sz		0
#endif

#ifdef CONFIG_ARCH_OMAP3
static struct resource omap3_sham_resources[] = {
	{
		.start	= OMAP34XX_SEC_SHA1MD5_BASE,
		.end	= OMAP34XX_SEC_SHA1MD5_BASE + 0x64,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= INT_34XX_SHA1MD52_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= OMAP34XX_DMA_SHA1MD5_RX,
		.flags	= IORESOURCE_DMA,
	}
};
static int omap3_sham_resources_sz = ARRAY_SIZE(omap3_sham_resources);
#else
#define omap3_sham_resources		NULL
#define omap3_sham_resources_sz		0
#endif

static struct platform_device sham_device = {
	.name		= "omap-sham",
	.id		= -1,
};

static void omap_init_sham(void)
{
	if (cpu_is_omap24xx()) {
		sham_device.resource = omap2_sham_resources;
		sham_device.num_resources = omap2_sham_resources_sz;
	} else if (cpu_is_omap34xx()) {
		sham_device.resource = omap3_sham_resources;
		sham_device.num_resources = omap3_sham_resources_sz;
	} else {
		pr_err("%s: platform not supported\n", __func__);
		return;
	}
	platform_device_register(&sham_device);
}
#else
static inline void omap_init_sham(void) { }
#endif

#if defined(CONFIG_CRYPTO_DEV_OMAP_AES) || defined(CONFIG_CRYPTO_DEV_OMAP_AES_MODULE)

#ifdef CONFIG_ARCH_OMAP2
static struct resource omap2_aes_resources[] = {
	{
		.start	= OMAP24XX_SEC_AES_BASE,
		.end	= OMAP24XX_SEC_AES_BASE + 0x4C,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= OMAP24XX_DMA_AES_TX,
		.flags	= IORESOURCE_DMA,
	},
	{
		.start	= OMAP24XX_DMA_AES_RX,
		.flags	= IORESOURCE_DMA,
	}
};
static int omap2_aes_resources_sz = ARRAY_SIZE(omap2_aes_resources);
#else
#define omap2_aes_resources		NULL
#define omap2_aes_resources_sz		0
#endif

#ifdef CONFIG_ARCH_OMAP3
static struct resource omap3_aes_resources[] = {
	{
		.start	= OMAP34XX_SEC_AES_BASE,
		.end	= OMAP34XX_SEC_AES_BASE + 0x4C,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= OMAP34XX_DMA_AES2_TX,
		.flags	= IORESOURCE_DMA,
	},
	{
		.start	= OMAP34XX_DMA_AES2_RX,
		.flags	= IORESOURCE_DMA,
	}
};
static int omap3_aes_resources_sz = ARRAY_SIZE(omap3_aes_resources);
#else
#define omap3_aes_resources		NULL
#define omap3_aes_resources_sz		0
#endif

static struct platform_device aes_device = {
	.name		= "omap-aes",
	.id		= -1,
};

static void omap_init_aes(void)
{
	if (cpu_is_omap24xx()) {
		aes_device.resource = omap2_aes_resources;
		aes_device.num_resources = omap2_aes_resources_sz;
	} else if (cpu_is_omap34xx()) {
		aes_device.resource = omap3_aes_resources;
		aes_device.num_resources = omap3_aes_resources_sz;
	} else {
		pr_err("%s: platform not supported\n", __func__);
		return;
	}
	platform_device_register(&aes_device);
}

#else
static inline void omap_init_aes(void) { }
#endif

/*-------------------------------------------------------------------------*/

#if defined(CONFIG_MMC_OMAP) || defined(CONFIG_MMC_OMAP_MODULE)

static inline void omap242x_mmc_mux(struct omap_mmc_platform_data
							*mmc_controller)
{
	if ((mmc_controller->slots[0].switch_pin > 0) && \
		(mmc_controller->slots[0].switch_pin < OMAP_MAX_GPIO_LINES))
		omap_mux_init_gpio(mmc_controller->slots[0].switch_pin,
					OMAP_PIN_INPUT_PULLUP);
	if ((mmc_controller->slots[0].gpio_wp > 0) && \
		(mmc_controller->slots[0].gpio_wp < OMAP_MAX_GPIO_LINES))
		omap_mux_init_gpio(mmc_controller->slots[0].gpio_wp,
					OMAP_PIN_INPUT_PULLUP);

	omap_mux_init_signal("sdmmc_cmd", 0);
	omap_mux_init_signal("sdmmc_clki", 0);
	omap_mux_init_signal("sdmmc_clko", 0);
	omap_mux_init_signal("sdmmc_dat0", 0);
	omap_mux_init_signal("sdmmc_dat_dir0", 0);
	omap_mux_init_signal("sdmmc_cmd_dir", 0);
	if (mmc_controller->slots[0].caps & MMC_CAP_4_BIT_DATA) {
		omap_mux_init_signal("sdmmc_dat1", 0);
		omap_mux_init_signal("sdmmc_dat2", 0);
		omap_mux_init_signal("sdmmc_dat3", 0);
		omap_mux_init_signal("sdmmc_dat_dir1", 0);
		omap_mux_init_signal("sdmmc_dat_dir2", 0);
		omap_mux_init_signal("sdmmc_dat_dir3", 0);
	}

	/*
	 * Use internal loop-back in MMC/SDIO Module Input Clock
	 * selection
	 */
	if (mmc_controller->slots[0].internal_clock) {
		u32 v = omap_ctrl_readl(OMAP2_CONTROL_DEVCONF0);
		v |= (1 << 24);
		omap_ctrl_writel(v, OMAP2_CONTROL_DEVCONF0);
	}
}

void __init omap242x_init_mmc(struct omap_mmc_platform_data **mmc_data)
{
	char *name = "mmci-omap";

	if (!mmc_data[0]) {
		pr_err("%s fails: Incomplete platform data\n", __func__);
		return;
	}

	omap242x_mmc_mux(mmc_data[0]);
	omap_mmc_add(name, 0, OMAP2_MMC1_BASE, OMAP2420_MMC_SIZE,
					INT_24XX_MMC_IRQ, mmc_data[0]);
}

#endif

static struct omap_rprm_regulator *omap_regulators;
static u32 omap_regulators_cnt;
void __init omap_rprm_regulator_init(struct omap_rprm_regulator *regulators,
				u32 regulator_cnt)
{
	if (!regulator_cnt)
		return;

	omap_regulators = regulators;
	omap_regulators_cnt = regulator_cnt;
}

u32 omap_rprm_get_regulators(struct omap_rprm_regulator **regulators)
{
	if (omap_regulators_cnt)
		*regulators = omap_regulators;

	return omap_regulators_cnt;
}
EXPORT_SYMBOL(omap_rprm_get_regulators);

static __init void omap_init_dev(char *name,
		struct omap_device_pm_latency *pm_lats, int pm_lats_cnt)
{
	struct platform_device *pd;
	struct omap_hwmod *oh;

	oh = omap_hwmod_lookup(name);
	if (!oh) {
		pr_err("Could not look up %s hwmod\n", name);
		return;
	}

	pd = omap_device_build(name, -1, oh, NULL, 0, pm_lats, pm_lats_cnt, 0);
	if (IS_ERR(pd))
		pr_err("Can't build omap_device for %s.\n", name);
	else
		pm_runtime_enable(&pd->dev);
}

int omap_cam_deactivate(struct omap_device *od)
{
	int i;

	for (i = 0; i < od->hwmods_cnt; i++)
		omap_hwmod_reset(od->hwmods[i]);

	return omap_device_idle_hwmods(od);
}

static struct omap_device_pm_latency omap_cam_latency[] = {
	{
		.deactivate_func = omap_cam_deactivate,
		.activate_func   = omap_device_enable_hwmods,
		.flags = OMAP_DEVICE_LATENCY_AUTO_ADJUST,
	}
};

static void __init omap_init_fdif(void)
{
	if (!cpu_is_omap44xx() && !cpu_is_omap54xx())
		return;

	omap_init_dev("fdif", omap_cam_latency, ARRAY_SIZE(omap_cam_latency));
}

static void __init omap_init_sl2if(void)
{
	if (!cpu_is_omap44xx() && !cpu_is_omap54xx())
		return;

	omap_init_dev("sl2if", NULL, 0);
}

static void __init omap_init_iss(void)
{
	if (!cpu_is_omap44xx() && !cpu_is_omap54xx())
		return;

	omap_init_dev("iss", omap_cam_latency, ARRAY_SIZE(omap_cam_latency));
}

/*-------------------------------------------------------------------------*/

#if defined(CONFIG_HDQ_MASTER_OMAP) || defined(CONFIG_HDQ_MASTER_OMAP_MODULE)
#define OMAP_HDQ_BASE	0x480B2000
static struct resource omap_hdq_resources[] = {
	{
		.start		= OMAP_HDQ_BASE,
		.end		= OMAP_HDQ_BASE + 0x1C,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= INT_24XX_HDQ_IRQ,
		.flags		= IORESOURCE_IRQ,
	},
};
static struct platform_device omap_hdq_dev = {
	.name = "omap_hdq",
	.id = 0,
	.dev = {
		.platform_data = NULL,
	},
	.num_resources	= ARRAY_SIZE(omap_hdq_resources),
	.resource	= omap_hdq_resources,
};
static inline void omap_hdq_init(void)
{
	if (cpu_is_omap2420())
		return;

	platform_device_register(&omap_hdq_dev);
}
#else
static inline void omap_hdq_init(void) {}
#endif

/*---------------------------------------------------------------------------*/

#if defined(CONFIG_VIDEO_OMAP2_VOUT) || \
	defined(CONFIG_VIDEO_OMAP2_VOUT_MODULE)
#if defined(CONFIG_FB_OMAP2) || defined(CONFIG_FB_OMAP2_MODULE)
static struct resource omap_vout_resource[3 - CONFIG_FB_OMAP2_NUM_FBS] = {
};
#else
static struct resource omap_vout_resource[2] = {
};
#endif

static struct platform_device omap_vout_device = {
	.name		= "omap_vout",
	.num_resources	= ARRAY_SIZE(omap_vout_resource),
	.resource 	= &omap_vout_resource[0],
	.id		= -1,
};
static void omap_init_vout(void)
{
	if (platform_device_register(&omap_vout_device) < 0)
		printk(KERN_ERR "Unable to register OMAP-VOUT device\n");
}
#else
static inline void omap_init_vout(void) {}
#endif


static int omap_device_path_need_mac(struct device *dev)
{
	const char **try = (const char **)mac_device_fixup_paths;
	const char *path;
	int count = count_mac_device_fixup_paths;
	const char *p;
	int len;
	struct device *devn;

	while (count--) {

		p = *try + strlen(*try);
		devn = dev;

		while (devn) {

			path = dev_name(devn);
			len = strlen(path);

			if ((p - *try) < len) {
				devn = NULL;
				continue;
			}

			p -= len;

			if (strncmp(path, p, len)) {
				devn = NULL;
				continue;
			}

			devn = devn->parent;
			if (p == *try)
				return count;

			if (devn != NULL && (p - *try) < 2)
				devn = NULL;

			p--;
			if (devn != NULL && *p != '/')
				devn = NULL;
		}

		try++;
	}

	return -ENOENT;
}

static int omap_panda_netdev_event(struct notifier_block *this,
                                                unsigned long event, void *ptr)
{
	struct net_device *dev = ptr;
	struct sockaddr sa;
	int n;

	if (event != NETDEV_REGISTER)
		return NOTIFY_DONE;

	n = omap_device_path_need_mac(dev->dev.parent);
	if (n >= 0) {
		sa.sa_family = dev->type;
		omap2_die_id_to_ethernet_mac(sa.sa_data, n);
		dev->netdev_ops->ndo_set_mac_address(dev, &sa);
	}

	return NOTIFY_DONE;
}

static struct notifier_block omap_panda_netdev_notifier = {
	.notifier_call = omap_panda_netdev_event,
	.priority = 1,
};


int omap_register_mac_device_fixup_paths(const char * const *paths, int count)
{
	mac_device_fixup_paths = paths;
	count_mac_device_fixup_paths = count;
	return register_netdevice_notifier(&omap_panda_netdev_notifier);
}

static struct omap_device_pm_latency omap_drm_latency[] = {
	[0] = {
		.deactivate_func	= omap_device_idle_hwmods,
		.activate_func		= omap_device_enable_hwmods,
		.flags			= OMAP_DEVICE_LATENCY_AUTO_ADJUST,
	},
};

/**
 * change_clock_parent - try to change a clock's parent
 * @dev: device pointer to change parent
 * @name: string containing the new requested parent's name
 */
static int change_clock_parent(struct device *dev, char *name)
{
	int ret;
	struct clk *fclk, *parent;

	fclk = clk_get(dev, "fck");
	if (IS_ERR_OR_NULL(fclk)) {
		dev_err(dev, "%s: %d: clk_get() FAILED\n",
				__func__, __LINE__);
		return -EINVAL;
	}

	parent = clk_get(dev, name);
	if (IS_ERR_OR_NULL(parent)) {
		dev_err(dev, "%s: %d: clk_get() %s FAILED\n",
			__func__, __LINE__, name);
		clk_put(fclk);
		return -EINVAL;
	}

	ret = clk_set_parent(fclk, parent);
	if (IS_ERR_VALUE(ret)) {
		dev_err(dev, "%s: clk_set_parent() to %s FAILED\n",
			__func__, name);
		ret = -EINVAL;
	}

	clk_put(parent);
	clk_put(fclk);

	return ret;
}

static void omap_init_gpu(void)
{
	struct omap_hwmod *oh;
	struct platform_device *od;
	struct gpu_platform_data *pdata;
	const char *oh_name = "gpu";
	char *name = "omapdrm_pvr";

	oh = omap_hwmod_lookup(oh_name);
	if (!oh) {
		pr_err("omap_init_gpu: Could not look up %s\n", oh_name);
		return;
	}

	pdata = kzalloc(sizeof(struct gpu_platform_data),
					GFP_KERNEL);
	if (!pdata) {
		pr_err("omap_init_gpu: Platform data memory allocation failed\n");
		return;
	}

	pdata->device_enable = omap_device_enable;
	pdata->device_idle = omap_device_idle;
	pdata->device_shutdown = omap_device_shutdown;

	od = omap_device_build(name, 0, oh, pdata,
			     sizeof(struct gpu_platform_data),
			     omap_drm_latency, ARRAY_SIZE(omap_drm_latency), 0);
	WARN(IS_ERR(od), "Could not build omap_device for %s %s\n",
	     name, oh_name);

	if (od && cpu_is_omap44xx()) {
		change_clock_parent(&((*od).dev), "dpll_per_m7x2_ck");
		pr_info("Updated GPU clock source to be dpll_per_m7x2_ck\n");
	}

	kfree(pdata);
}

/*-------------------------------------------------------------------------*/

static int __init omap2_init_devices(void)
{
	/*
	 * please keep these calls, and their implementations above,
	 * in alphabetical order so they're easier to sort through.
	 */
	omap_init_abe();
	omap_init_aess();
	omap_init_audio();
	omap_init_mcasp();
	omap_init_dmic();
	omap_init_mcpdm();
	omap_init_camera();
	omap3_init_camera(&bogus_isp_pdata);
	omap_init_hdmi_audio();
	omap_init_mbox();
	omap_init_mcspi();
	omap_init_pmu();
	omap_hdq_init();
	omap_init_sti();
	omap_init_sham();
#if defined(CONFIG_SATA_AHCI_PLATFORM) || \
	defined(CONFIG_SATA_AHCI_PLATFORM_MODULE)
	omap_sata_init();
#endif
	omap_init_aes();
	omap_init_vout();
	omap_init_fdif();
	omap_init_sl2if();
	omap_init_iss();
	omap_init_gpu();

	return 0;
}
arch_initcall(omap2_init_devices);

#if defined(CONFIG_OMAP_WATCHDOG) || defined(CONFIG_OMAP_WATCHDOG_MODULE)
static int __init omap_init_wdt(void)
{
	int id = -1;
	struct platform_device *pdev;
	struct omap_hwmod *oh;
	char *oh_name = "wd_timer2";
	char *dev_name = "omap_wdt";

	if (!cpu_class_is_omap2())
		return 0;

	oh = omap_hwmod_lookup(oh_name);
	if (!oh) {
		pr_err("Could not look up wd_timer%d hwmod\n", id);
		return -EINVAL;
	}

	pdev = omap_device_build(dev_name, id, oh, NULL, 0, NULL, 0, 0);
	WARN(IS_ERR(pdev), "Can't build omap_device for %s:%s.\n",
				dev_name, oh->name);
	return 0;
}
subsys_initcall(omap_init_wdt);
#endif
