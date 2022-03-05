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
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>

#include <linux/mm_types.h>
#include <linux/profile.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/mmu_notifier.h>
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

// GET PAGE FAULTS
uint64_t dec_get_tlb_fault(uint64_t tile) {
  uint64_t res = *(volatile uint64_t *)(get_tlb_fault | mmub);
  return res;
}

// FLUSH TLB
void dec_flush_tlb (struct mmu_notifier *mn,
				                            struct mm_struct *mm,
				                            unsigned long start, unsigned long end) {
   *(volatile uint64_t*)(tlb_flush | mmub);
   
}

// RESOLVE PAGE FAULT, but not load entry into TLB, let PTW do it
void dec_resolve_page_fault(uint64_t tile) {
  // followed by maple, treat_int()
  uint64_t res = dec_get_tlb_fault(0);
  uint64_t conf_tlb_entry = res & 0xF;

  *(volatile uint64_t*)(resolve_tlb_fault | mmub) = conf_tlb_entry; 
}

static int irq;

static struct mm_struct *curr_mm;

static const struct mmu_notifier_ops iommu_mn = {
	// maple API is used
	.invalidate_range       = dec_flush_tlb,
};

static struct mmu_notifier mn = {
	.ops = &iommu_mn,
}; 

static irqreturn_t cohort_mmu_interrupt(int irq, void *dev_id){
	// maple API is used
	dec_resolve_page_fault(COHORT_TILE);

	return IRQ_HANDLED;
};

void cohort_mn_register(struct mm_struct *mm){
	printk("Cohort MMU register fun entered! \n");

	curr_mm = mm;
	mn.mm = mm;

	printk("Cohort MMU: register fun called! \n");
	mmu_notifier_register(&mn, curr_mm);
	printk("Cohort MMU: register fun returned! \n");

}
EXPORT_SYMBOL(cohort_mn_register);

static int cohort_mmu_probe(struct platform_device *ofdev)
{	
	pr_info("Cohort MMU Driver Probe v1.7 entered!\n");

	struct device *dev = &ofdev->dev;
	
	dev_info(dev, "Cohort Device Structure extracted!\n");

	/* Get IRQ for the device */
	struct resource *res;
	res = platform_get_resource(ofdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_err(dev, "no IRQ found\n");
		return -1;
	}

	dev_info(dev, "Cohort IRQ res name: %s\n", res->name);

	irq = res->start;

	// listen for interrupts for a page fault
	int retval;
	retval = request_irq(irq, cohort_mmu_interrupt, 0, dev_name(dev), dev);

	dev_info(dev, "Cohort IRQ return value: %d", retval);

	if (retval < 0){
		dev_err(dev, "Can't request IRQ with retval: %d\n", retval);
		return retval;
	}

	dev_info(dev, "Cohort Probe is successfully launched!\n");

	return 0;

}

static int cohort_mmu_remove(struct platform_device *ofdev){
	pr_info("Cohort MMU Driver Remove\n");

	mmu_notifier_unregister(&mn, curr_mm);

	pr_info("Cohort MMU Unregistered\n");

	struct device *dev = &ofdev->dev;
	// pr_info("Cohort Device info(2):%s\n", dev->init_name);
	pr_info("Cohort Device info(2):\n");
	free_irq(irq, dev);

	pr_info("Cohort MMU IRQ freed and leaving!\n");
	
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