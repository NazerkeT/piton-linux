#include <stdint.h>

#define BYTE         8

#define MAX_TILES    1
#define BASE_MMU     0xe100A00000
#define TILE_X       1
#define TILE_Y       1
#define WIDTH        1 // --> clarify this
#define FIFO         2 // --> clarify this
#define COHORT_TILE  1  // x=1, y=0

static uint64_t tlb_flush            = (uint64_t)(13*BYTE);
static uint64_t resolve_tlb_fault    = (uint64_t)(15*BYTE | 2 << FIFO);
static uint64_t get_tlb_fault        = (uint64_t)(13*BYTE | 1 << FIFO);
static uint64_t mmub[MAX_TILES];

mmub[COHORT_TILE] = BASE_MMU | ((COHORT_TILE%WIDTH) << TILE_X) | ((0) << TILE_Y); 

// GET PAGE FAULTS
uint64_t dec_get_tlb_fault(uint64_t tile) {
  uint64_t res = *(volatile uint64_t *)(get_tlb_fault | mmub[tile]);
  return res;
}

// FLUSH TLB
uint64_t dec_flush_tlb (uint64_t tile) {
   uint64_t res = *(volatile uint64_t*)(tlb_flush | mmub[tile]);
   return res;
}

// RESOLVE PAGE FAULT, but not load entry into TLB, let PTW do it
void dec_resolve_page_fault(uint64_t tile) {
  // followed by maple, treat_int()
  uint64_t res = dec_get_tlb_fault(0);
  uint64_t conf_tlb_entry = res & 0xF;

  *(volatile uint64_t*)(resolve_tlb_fault | mmub[tile]) = conf_tlb_entry; 
}