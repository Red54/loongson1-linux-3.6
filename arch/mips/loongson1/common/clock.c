/*
 * Copyright (c) 2011 Zhang, Keguang <keguang.zhang@gmail.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/module.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <asm/clock.h>
#include <asm/time.h>

#include <loongson1.h>

static LIST_HEAD(clocks);
static DEFINE_MUTEX(clocks_mutex);

struct clk *clk_get(struct device *dev, const char *name)
{
	struct clk *c;
	struct clk *ret = NULL;

	mutex_lock(&clocks_mutex);
	list_for_each_entry(c, &clocks, node) {
		if (!strcmp(c->name, name)) {
			ret = c;
			break;
		}
	}
	mutex_unlock(&clocks_mutex);

	return ret;
}
EXPORT_SYMBOL(clk_get);

int clk_enable(struct clk *clk)
{
	return 0;
}
EXPORT_SYMBOL(clk_enable);

void clk_disable(struct clk *clk)
{
}
EXPORT_SYMBOL(clk_disable);

unsigned long clk_get_rate(struct clk *clk)
{
	return clk->rate;
}
EXPORT_SYMBOL(clk_get_rate);

void clk_put(struct clk *clk)
{
}
EXPORT_SYMBOL(clk_put);

static void pll_clk_init(struct clk *clk)
{
	u32 pll;

	pll = __raw_readl(LS1X_CLK_PLL_FREQ);
#if defined(CONFIG_LOONGSON1_LS1C)
	clk->rate = (((pll >> 8) & 0xff) + ((pll >> 16) & 0xff)) * APB_CLK / 4;
#else
	clk->rate = (12 + (pll & 0x3f)) * APB_CLK / 2
			+ ((pll >> 8) & 0x3ff) * APB_CLK / 1024 / 2;
#endif
}

static void cpu_clk_init(struct clk *clk)
{
	u32 pll, ctrl;

	pll = clk_get_rate(clk->parent);
	ctrl = __raw_readl(LS1X_CLK_PLL_DIV);
#if defined(CONFIG_LOONGSON1_LS1A)
	/* 由于目前loongson 1A CPU读取0xbfe78030 PLL寄存器有问题，
	   所以CPU的频率是通过PMON传进来的 */
	clk->rate = cpu_clock_freq;
#elif defined(CONFIG_LOONGSON1_LS1B)
	clk->rate = pll / ((ctrl & DIV_CPU) >> DIV_CPU_SHIFT);
#else
	if (ctrl & DIV_CPU_SEL) {
		if(ctrl & DIV_CPU_EN) {
			clk->rate = pll / ((ctrl & DIV_CPU) >> DIV_CPU_SHIFT);
		} else {
			clk->rate = pll / 2;
		}
	} else {
		clk->rate = APB_CLK;
	}
#endif
}

static void ddr_clk_init(struct clk *clk)
{
	u32 pll, ctrl;

	pll = clk_get_rate(clk->parent);
	ctrl = __raw_readl(LS1X_CLK_PLL_DIV);
#if defined(CONFIG_LOONGSON1_LS1A)
	/* 由于目前loongson 1A CPU读取0xbfe78030 PLL寄存器有问题，
	   所以BUS(DDR)的频率是通过PMON传进来的 */
	clk->rate = ls1x_bus_clock;
#elif defined(CONFIG_LOONGSON1_LS1B)
	clk->rate = pll / ((ctrl & DIV_DDR) >> DIV_DDR_SHIFT);
#else
	ctrl = __raw_readl(LS1X_CLK_PLL_FREQ) & 0x3;
	switch	 (ctrl) {
		case 0:
			clk->rate = pll / 2;
		break;
		case 1:
			clk->rate = pll / 4;
		break;
		case 2:
		case 3:
			clk->rate = pll / 3;
		break;
	}
#endif
}

static void apb_clk_init(struct clk *clk)
{
	u32 pll;

	pll = clk_get_rate(clk->parent);
#if defined(CONFIG_LOONGSON1_LS1C)
	clk->rate = pll;
#else
	clk->rate = pll / 2;
#endif
}

static void dc_clk_init(struct clk *clk)
{
	u32 pll, ctrl;

	pll = clk_get_rate(clk->parent);
	ctrl = __raw_readl(LS1X_CLK_PLL_DIV) & DIV_DC;
	clk->rate = pll / (ctrl >> DIV_DC_SHIFT);
}

static struct clk_ops pll_clk_ops = {
	.init	= pll_clk_init,
};

static struct clk_ops cpu_clk_ops = {
	.init	= cpu_clk_init,
};

static struct clk_ops ddr_clk_ops = {
	.init	= ddr_clk_init,
};

static struct clk_ops apb_clk_ops = {
	.init	= apb_clk_init,
};

static struct clk_ops dc_clk_ops = {
	.init	= dc_clk_init,
};

static struct clk pll_clk = {
	.name	= "pll",
	.ops	= &pll_clk_ops,
};

static struct clk cpu_clk = {
	.name	= "cpu",
	.parent = &pll_clk,
	.ops	= &cpu_clk_ops,
};

static struct clk ddr_clk = {
	.name	= "ddr",
#if defined(CONFIG_LOONGSON1_LS1C)
	.parent = &cpu_clk,
#else
	.parent = &pll_clk,
#endif
	.ops	= &ddr_clk_ops,
};

static struct clk apb_clk = {
	.name	= "apb",
	.parent = &ddr_clk,
	.ops	= &apb_clk_ops,
};

static struct clk dc_clk = {
	.name	= "dc",
	.parent = &pll_clk,
	.ops	= &dc_clk_ops,
};

int clk_register(struct clk *clk)
{
	mutex_lock(&clocks_mutex);
	list_add(&clk->node, &clocks);
	if (clk->ops->init)
		clk->ops->init(clk);
	mutex_unlock(&clocks_mutex);

	return 0;
}
EXPORT_SYMBOL(clk_register);

static struct clk *ls1x_clks[] = {
	&pll_clk,
	&cpu_clk,
	&ddr_clk,
	&apb_clk,
	&dc_clk,
};

int __init ls1x_clock_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ls1x_clks); i++)
		clk_register(ls1x_clks[i]);

	return 0;
}

void __init plat_time_init(void)
{
	struct clk *clk;

	/* Initialize LS1X clocks */
	ls1x_clock_init();

	/* setup mips r4k timer */
	clk = clk_get(NULL, "cpu");
	if (IS_ERR(clk))
		panic("unable to get dc clock, err=%ld", PTR_ERR(clk));

	mips_hpt_frequency = clk_get_rate(clk) / 2;
}
