/*
 * Copyright (c) 2011 Zhang, Keguang <keguang.zhang@gmail.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/phy.h>
#include <linux/serial_8250.h>
#include <linux/stmmac.h>
#include <asm-generic/sizes.h>

#include <loongson1.h>
#include <nand.h>

static struct mtd_partition ls1x_nand_partitions[] = {
	{
		.name	= "kernel",
		.offset	= MTDPART_OFS_APPEND,
		.size	= 14*1024*1024,
	},  {
		.name	= "rootfs",
		.offset	= MTDPART_OFS_APPEND,
		.size	= 100*1024*1024,
	},  {
		.name	= "data",
		.offset	= MTDPART_OFS_APPEND,
		.size	= MTDPART_SIZ_FULL,
	},
};

static struct ls1x_nand_platform_data ls1x_nand_parts = {
	.parts		= ls1x_nand_partitions,
	.nr_parts	= ARRAY_SIZE(ls1x_nand_partitions),
};

static struct resource ls1x_nand_resources[] = {
	[0] = {
		.start	= LS1X_NAND_BASE,
		.end	= LS1X_NAND_BASE + SZ_16K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= LS1X_DMA0_IRQ,
		.end	= LS1X_DMA0_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device ls1x_nand_device = {
	.name	= "ls1x-nand",
	.id	= -1,
	.dev	= {
		.platform_data = &ls1x_nand_parts,
	},
	.num_resources	= ARRAY_SIZE(ls1x_nand_resources),
	.resource	= ls1x_nand_resources,
};

#define LS1X_UART(_id)						\
	{							\
		.mapbase	= LS1X_UART ## _id ## _BASE,	\
		.irq		= LS1X_UART ## _id ## _IRQ,	\
		.iotype		= UPIO_MEM,			\
		.flags		= UPF_IOREMAP | UPF_FIXED_TYPE,	\
		.type		= PORT_16550A,			\
	}

static struct plat_serial8250_port ls1x_serial8250_port[] = {
	LS1X_UART(0),
	LS1X_UART(1),
	LS1X_UART(2),
	LS1X_UART(3),
	{},
};

struct platform_device ls1x_uart_device = {
	.name		= "serial8250",
	.id		= PLAT8250_DEV_PLATFORM,
	.dev		= {
		.platform_data = ls1x_serial8250_port,
	},
};

void __init ls1x_serial_setup(void)
{
	struct clk *clk;
	struct plat_serial8250_port *p;

	clk = clk_get(NULL, "apb");
	if (IS_ERR(clk))
		panic("unable to get apb clock, err=%ld", PTR_ERR(clk));

	for (p = ls1x_serial8250_port; p->flags != 0; ++p)
		p->uartclk = clk_get_rate(clk);
}

/* Synopsys Ethernet GMAC */
static struct resource ls1x_eth0_resources[] = {
	[0] = {
		.start	= LS1X_GMAC0_BASE,
		.end	= LS1X_GMAC0_BASE + SZ_64K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.name	= "macirq",
		.start	= LS1X_GMAC0_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct stmmac_mdio_bus_data ls1x_mdio_bus_data = {
	.bus_id		= 0,
	.phy_mask	= 0,
};

static struct plat_stmmacenet_data ls1x_eth_data = {
	.bus_id		= 0,
	.phy_addr	= -1,
	.mdio_bus_data	= &ls1x_mdio_bus_data,
	.has_gmac	= 1,
	.tx_coe		= 1,
};

struct platform_device ls1x_eth0_device = {
	.name		= "stmmaceth",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(ls1x_eth0_resources),
	.resource	= ls1x_eth0_resources,
	.dev		= {
		.platform_data = &ls1x_eth_data,
	},
};

/* USB OHCI */
#ifdef CONFIG_USB_OHCI_HCD_PLATFORM
#include <linux/usb/ohci_pdriver.h>
static u64 ls1x_ohci_dmamask = DMA_BIT_MASK(32);

static struct resource ls1x_ohci_resources[] = {
	[0] = {
		.start	= LS1X_OHCI_BASE,
		.end	= LS1X_OHCI_BASE + SZ_32K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= LS1X_OHCI_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct usb_ohci_pdata ls1x_ohci_pdata = {
};

struct platform_device ls1x_ohci_device = {
	.name		= "ohci-platform",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(ls1x_ohci_resources),
	.resource	= ls1x_ohci_resources,
	.dev		= {
		.dma_mask = &ls1x_ohci_dmamask,
		.platform_data = &ls1x_ohci_pdata,
	},
};
#endif

/* USB EHCI */
static u64 ls1x_ehci_dmamask = DMA_BIT_MASK(32);

static struct resource ls1x_ehci_resources[] = {
	[0] = {
		.start	= LS1X_EHCI_BASE,
		.end	= LS1X_EHCI_BASE + SZ_32K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= LS1X_EHCI_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device ls1x_ehci_device = {
	.name		= "ls1x-ehci",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(ls1x_ehci_resources),
	.resource	= ls1x_ehci_resources,
	.dev		= {
		.dma_mask = &ls1x_ehci_dmamask,
	},
};

/* Real Time Clock */
struct platform_device ls1x_rtc_device = {
	.name		= "ls1x-rtc",
	.id		= -1,
};
