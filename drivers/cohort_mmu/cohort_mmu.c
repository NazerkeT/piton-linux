/*
 * Copyright (C) 2010-2012 Advanced Micro Devices, Inc.
 * Author: Joerg Roedel <jroedel@suse.de>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <linux/mmu_notifier.h>
#include <linux/amd-iommu.h>
#include <linux/mm_types.h>
#include <linux/profile.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/iommu.h>
#include <linux/wait.h>
#include <linux/pci.h>
#include <linux/gfp.h>
#include <linux/interrupt.h>
#include <linux/skbuff.h>
#include <linux/ioport.h> // does not link somehow

#include "cohort_mmu.h"

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Nazerke Turtayeva <nturtayeva@ucsb.edu>");

#define DRIVER_NAME "cohort_mmu"

static int irq;

static struct mm_struct curr_mm;

static struct mmu_notifier mn; 

static const struct mmu_notifier_ops iommu_mn = {
	// maple API is used
	.invalidate_range       = dec_flush_tlb(COHORT_TILE);
};

static irqreturn_t cohort_mmu_interrupt(int irq, void *dev_id){
	// maple API is used
	dec_resolve_page_fault(COHORT_TILE);

	return IRQ_HANDLED;
}

static void cohort_mn_register(struct mm_struct *mm){
	curr_mm = &mm;
	mn.ops  = &iommu_mn;

	mmu_notifier_register(&mn, mm);

}
EXPORT_SYMBOL(cohort_mn_register);

static int cohort_mmu_probe(struct platform_device *ofdev)
{	
	pr_info("Cohort MMU driver\n");

	int retval;

	struct device *dev = &ofdev->dev;

	/* Get IRQ for the device */
	struct resource *res;
	res = platform_get_resource(ofdev, IORESOURCE_IRQ, 0);
	if (!res) {
		pr_info("no IRQ found\n");
		dev_err(dev, "no IRQ found\n");
		return;
	}

	irq = res->start;

	// listen for interrupts for a page fault
	retval = request_irq(irq, cohort_mmu_interrupt, 0, "coh-mmu", dev);

	if (retval)
		pr_err("Can't request irq\n");

	return 0;

}

static int cohort_mmu_remove(struct platform_device *ofdev){
	mmu_notifier_unregister(&iommu_mn, *curr_mm);

	struct device *dev = &ofdev->dev;
	free_irq(irq, dev);
	
	return 0;
}

/* Match table for OF platform binding */
static const struct of_device_id cohort_of_match[] = {
	{ .compatible = "ucsbarchlab,cohort-0.0.a", },
    { /* end of list */ },
};
MODULE_DEVICE_TABLE(of, cohort_of_match);

static struct platform_driver cohort_of_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = cohort_of_match,
	},
	.probe		= cohort_mmu_probe,
	.remove		= cohort_mmu_remove,
};

module_platform_driver(cohort_of_driver);