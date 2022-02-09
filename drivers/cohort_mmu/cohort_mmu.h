// #include <stdint.h>
#ifndef _COHORT_MMU_H
#define _COHORT_MMU_H

#include <linux/sched/mm.h>
#include <linux/mmu_notifier.h>
#include <linux/iommu.h>

#define BYTE         8

#define MAX_TILES    2
#define BASE_MMU     0xe100A00000L
#define TILE_X       1
#define TILE_Y       1
#define WIDTH        1 // --> clarify this
#define FIFO         2 // --> clarify this
#define COHORT_TILE  1  // x=1, y=0

static uint64_t tlb_flush            = (uint64_t)(13*BYTE);
static uint64_t resolve_tlb_fault    = (uint64_t)(15*BYTE | 2 << FIFO);
static uint64_t get_tlb_fault        = (uint64_t)(13*BYTE | 1 << FIFO);
static uint64_t mmub = BASE_MMU | ((COHORT_TILE%WIDTH) << TILE_X) | ((0) << TILE_Y); 

uint64_t dec_get_tlb_fault(uint64_t tile);
void dec_flush_tlb (struct mmu_notifier *mn,
				                            struct mm_struct *mm,
				                            unsigned long start, unsigned long end);
void dec_resolve_page_fault(uint64_t tile);

extern void cohort_mn_register(struct mm_struct *mm);

#endif