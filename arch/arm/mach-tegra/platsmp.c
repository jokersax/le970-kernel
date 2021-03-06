/*
 *  linux/arch/arm/mach-tegra/platsmp.c
 *
 *  Copyright (C) 2002 ARM Ltd.
 *  All Rights Reserved
 *
 *  Copyright (C) 2009 Palm
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/smp.h>
#include <linux/io.h>

#include <asm/cacheflush.h>
#include <asm/hardware/gic.h>
#include <mach/hardware.h>
#include <asm/mach-types.h>
#include <asm/smp_scu.h>

#include <mach/iomap.h>

extern void tegra_secondary_startup(void);

static DEFINE_SPINLOCK(boot_lock);
static void __iomem *scu_base = IO_ADDRESS(TEGRA_ARM_PERIF_BASE);

#define EVP_CPU_RESET_VECTOR \
	(IO_ADDRESS(TEGRA_EXCEPTION_VECTORS_BASE) + 0x100)
#define CLK_RST_CONTROLLER_CLK_CPU_CMPLX \
	(IO_ADDRESS(TEGRA_CLK_RESET_BASE) + 0x4c)
#define CLK_RST_CONTROLLER_RST_CPU_CMPLX_CLR \
	(IO_ADDRESS(TEGRA_CLK_RESET_BASE) + 0x344)

void platform_secondary_init(unsigned int cpu)
{
	/*
	 * if any interrupts are already enabled for the primary
	 * core (e.g. timer irq), then they will not have been enabled
	 * for us: do so
	 */
	gic_secondary_init(0);

	/*
	 * Synchronise with the boot thread.
	 */
	spin_lock(&boot_lock);
	spin_unlock(&boot_lock);
}

int boot_secondary(unsigned int cpu, struct task_struct *idle)
{
	unsigned long old_boot_vector;
	unsigned long boot_vector;
	unsigned long timeout;
	u32 reg;

	/*
	 * set synchronisation state between this boot processor
	 * and the secondary one
	 */
	spin_lock(&boot_lock);


	/* set the reset vector to point to the secondary_startup routine */

	boot_vector = virt_to_phys(tegra_secondary_startup);
	old_boot_vector = readl(EVP_CPU_RESET_VECTOR);
	writel(boot_vector, EVP_CPU_RESET_VECTOR);

	/* enable cpu clock on cpu1 */
	reg = readl(CLK_RST_CONTROLLER_CLK_CPU_CMPLX);
	writel(reg & ~(1<<9), CLK_RST_CONTROLLER_CLK_CPU_CMPLX);

	reg = (1<<13) | (1<<9) | (1<<5) | (1<<1);
	writel(reg, CLK_RST_CONTROLLER_RST_CPU_CMPLX_CLR);

	smp_wmb();
	flush_cache_all();

	/* unhalt the cpu */
	writel(0, IO_ADDRESS(TEGRA_FLOW_CTRL_BASE) + 0x14);

	timeout = jiffies + (1 * HZ);
	while (time_before(jiffies, timeout)) {
		if (readl(EVP_CPU_RESET_VECTOR) != boot_vector)
			break;
		udelay(10);
	}

	/* put the old boot vector back */
	writel(old_boot_vector, EVP_CPU_RESET_VECTOR);

	/*
	 * now the secondary core is starting up let it run its
	 * calibrations, then wait for it to finish
	 */
	spin_unlock(&boot_lock);

	return 0;
}

/*
 * Initialise the CPU possible map early - this describes the CPUs
 * which may be present or become present in the system.
 */
void __init smp_init_cpus(void)
{
	unsigned int i, ncores = scu_get_core_count(scu_base);

	if (ncores > NR_CPUS) {
		printk(KERN_ERR "Tegra: no. of cores (%u) greater than configured (%u), clipping\n",
			ncores, NR_CPUS);
		ncores = NR_CPUS;
	}

	for (i = 0; i < ncores; i++)
		cpu_set(i, cpu_possible_map);

	set_smp_cross_call(gic_raise_softirq);
}

void __init platform_smp_prepare_cpus(unsigned int max_cpus)
{

	scu_enable(scu_base);
}
