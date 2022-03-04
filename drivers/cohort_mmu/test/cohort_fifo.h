#ifndef COHORT_FIFO_PARAM_H
#define COHORT_FIFO_PARAM_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <malloc.h>
#include <string.h>
#include <stdbool.h>
#include "dcpn.h"
#ifndef BARE_METAL
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#endif

#ifdef PRI
#define PRINTBT printf("%s\n", __func__);
#else
#define PRINTBT 
#endif

// code for Benchmarks

void cohort_print_monitors();

/**
 * an uncached write that applies to bare metal and linux alike
 */
void baremetal_write(uint32_t tile, uint64_t addr, uint64_t value);

uint64_t uncached_read(uint32_t tile, uint64_t addr);
/**
 * turn on cohort
 */
void cohort_on();

/**
 * turn off cohort
 */
void cohort_off();

typedef uint32_t addr_t; // though we only use the lower 32 bits
typedef uint32_t len_t; // length of fifo
typedef len_t ptr_t;
typedef uint32_t el_size_t; // element size width

struct _fifo_ctrl_t;

typedef struct _fifo_ctrl_t fifo_ctrl_t;

/*
 * generic type for fifo_push functions and pop functions
 * actual memory operation size depends on the configured fifo size
 * always wrap in 64 bits
 */
typedef void (*fifo_push_func_t)(uint64_t element, fifo_ctrl_t* fifo_ctrl);
typedef uint64_t (*fifo_pop_func_t)(fifo_ctrl_t* fifo_ctrl);


typedef struct __attribute__((__packed__)) {
    addr_t addr;
    addr_t addr_upper;
    el_size_t size;
    len_t len;
} meta_t;

struct _fifo_ctrl_t {
    uint32_t fifo_length;
    uint32_t element_size;
    ptr_t* head_ptr;
    ptr_t* tail_ptr;
    meta_t* meta_ptr;
    void* data_array;
    
    // function pointers to actual implemented function
    // decided at class instantiation time
    fifo_push_func_t fifo_push_func;
    fifo_pop_func_t fifo_pop_func;
    
};


void fifo_push_8 (uint64_t element, fifo_ctrl_t* fifo_ctrl);
void fifo_push_16(uint64_t element, fifo_ctrl_t* fifo_ctrl);
void fifo_push_32(uint64_t element, fifo_ctrl_t* fifo_ctrl);
void fifo_push_64(uint64_t element, fifo_ctrl_t* fifo_ctrl);
uint64_t fifo_pop_8 (fifo_ctrl_t* fifo_ctrl);
uint64_t fifo_pop_16 (fifo_ctrl_t* fifo_ctrl);
uint64_t fifo_pop_32 (fifo_ctrl_t* fifo_ctrl);
uint64_t fifo_pop_64 (fifo_ctrl_t* fifo_ctrl);

uint16_t clog2(uint16_t el);

//TODO: 128 bits are not supported, see https://github.com/rust-lang/rust/issues/54341

void fifo_start(fifo_ctrl_t *fifo_ctrl, bool is_consumer);

/**
 *@fifo_length: the length of the fifo, in bytes
 *@element_size: the size of each element in fifo, in bytes
 *@is_consumer: if it's true, then it is a consumer; otherwise it's a producer. It's used to calculate uncached_write offset. The software producer thread produces into 0-2, the other produces to 3-5
 */
fifo_ctrl_t *fifo_init(uint32_t fifo_length, uint16_t element_size, bool is_consumer)
{
    PRINTBT
    // 128 is the cache line width of openpiton
    fifo_ctrl_t *fifo_ctrl = (fifo_ctrl_t *) malloc(sizeof(fifo_ctrl_t));

    fifo_ctrl->head_ptr =   memalign(128, 128);
    fifo_ctrl->meta_ptr =   memalign(128, 128);
    fifo_ctrl->tail_ptr =   memalign(128, 128);
    fifo_ctrl->data_array = memalign(128, 128 * fifo_length);

#ifdef PRI
    printf("fhead %x\n", fifo_ctrl->head_ptr);
    printf("fmeta %x\n", fifo_ctrl->meta_ptr);
    printf("ftail %x\n", fifo_ctrl->tail_ptr);
    printf("fdata %x\n", fifo_ctrl->data_array);
#endif

    fifo_ctrl->fifo_length = fifo_length;
    fifo_ctrl->element_size = (element_size / 8);
    
    fifo_push_func_t fifo_push_func;
    fifo_pop_func_t fifo_pop_func;

    switch(element_size) {
        case 8:
            printf("Incompatible element size! Exiting\n");
            fifo_push_func = &fifo_push_8;
            fifo_pop_func = &fifo_pop_8;
            break;
        case 16:
            printf("Incompatible element size! Exiting\n");
            fifo_push_func = &fifo_push_16;
            fifo_pop_func = &fifo_pop_16;
            break;
        case 32:
            printf("Incompatible element size! Exiting\n");
            fifo_push_func = &fifo_push_32;
            fifo_pop_func = &fifo_pop_32;
            break;
        case 64:
            fifo_push_func = &fifo_push_64;
            fifo_pop_func = &fifo_pop_64;
            break;

        default:
            printf("Incompatible element size! Exiting\n");

    }

    fifo_ctrl->fifo_push_func = fifo_push_func;
    fifo_ctrl->fifo_pop_func = fifo_pop_func;

    fifo_start(fifo_ctrl, is_consumer);

    //TODO: use generic push/pop here
    return fifo_ctrl;
}

void fifo_start(fifo_ctrl_t *fifo_ctrl, bool is_consumer)
{
    PRINTBT
    *(fifo_ctrl->tail_ptr) = 0x00000000ULL;
    *(fifo_ctrl->head_ptr) = 0x00000000ULL;
    fifo_ctrl->meta_ptr->addr = (uint64_t) fifo_ctrl->data_array;
    fifo_ctrl->meta_ptr->len = fifo_ctrl->fifo_length;
    fifo_ctrl->meta_ptr->size = fifo_ctrl->element_size;
    memset(fifo_ctrl->data_array, 0, 128 * fifo_ctrl->fifo_length);
    __sync_synchronize;
    if (is_consumer) {
        baremetal_write( 0, 3,( uint64_t )fifo_ctrl->head_ptr);
        baremetal_write( 0, 4,( uint64_t )fifo_ctrl->meta_ptr);
        baremetal_write( 0, 5,( uint64_t )fifo_ctrl->tail_ptr);
    } else {
        baremetal_write( 0, 0,( uint64_t ) fifo_ctrl->tail_ptr);
        baremetal_write( 0, 1,( uint64_t ) fifo_ctrl->meta_ptr);
        baremetal_write( 0, 2,( uint64_t ) fifo_ctrl->head_ptr);
    }
    __sync_synchronize;
}

ptr_t private_get_incremented_tail(fifo_ctrl_t *fifo_ctrl)
{
    PRINTBT
    return (*(fifo_ctrl->tail_ptr) + 1 ) % (fifo_ctrl->fifo_length);
}

ptr_t private_get_incremented_head(fifo_ctrl_t *fifo_ctrl)
{
    PRINTBT
    return (*(fifo_ctrl->head_ptr) + 1 ) % (fifo_ctrl->fifo_length);
}

ptr_t private_get_tail(fifo_ctrl_t *fifo_ctrl)
{
    PRINTBT
    return *((ptr_t *)(fifo_ctrl->tail_ptr));
}

ptr_t private_get_head(fifo_ctrl_t *fifo_ctrl)
{
    PRINTBT
    return *((ptr_t *)(fifo_ctrl->head_ptr));
}


/**
 *@return: 0 if not empty, 1 if empty
 **/
int fifo_is_empty(fifo_ctrl_t* fifo_ctrl)
{
 #ifdef PRI
    printf("%s: the head ptr is %lx\n", __func__, private_get_head(fifo_ctrl));
    printf("%s: the tail ptr is %lx\n", __func__, private_get_tail(fifo_ctrl));
#endif
   return private_get_tail(fifo_ctrl) == private_get_head(fifo_ctrl);
}

addr_t fifo_get_base(fifo_ctrl_t fifo_ctrl)
{
    PRINTBT
    return (uint64_t) fifo_ctrl.data_array;
}

/**
 *@returns: 0 if not full, 1 if full */
int fifo_is_full(fifo_ctrl_t* fifo_ctrl)
{
#ifdef PRI
    printf("%s: the head ptr is %lx\n", __func__, private_get_head(fifo_ctrl));
    printf("%s: the tail ptr is %lx\n", __func__, private_get_tail(fifo_ctrl));
#endif
    return private_get_incremented_tail(fifo_ctrl) == private_get_head(fifo_ctrl);
}

void fifo_deinit(fifo_ctrl_t *fifo_ctrl)
{
    PRINTBT
    // first free the large data array
    free(fifo_ctrl->data_array);

    // then free 3 cachelines
    free(fifo_ctrl->head_ptr);
    free(fifo_ctrl->tail_ptr);
    free(fifo_ctrl->meta_ptr);

    // at long last free the fifo pointer
    free(fifo_ctrl);
}


// philosophy: don't chunk requests in software for better transparency
// as 128 bits aren't supported, 64 would suffice
//
//
void fifo_push_64(uint64_t element, fifo_ctrl_t* fifo_ctrl)
{
    PRINTBT
    // loop whilie the fifo is full
#ifndef PRI
    while (fifo_is_full(fifo_ctrl));
#else
    if (fifo_is_full(fifo_ctrl)) {
        printf("fifo is full\n");
        return;
    }
#endif
    *(((uint64_t *) fifo_ctrl->data_array) + (*fifo_ctrl->tail_ptr * 2)) = (uint64_t) element;
    __sync_synchronize;
    *(fifo_ctrl->tail_ptr) = private_get_incremented_tail(fifo_ctrl);
}

uint64_t fifo_pop_64(fifo_ctrl_t *fifo_ctrl)
{
    PRINTBT
#ifndef PRI
    while (fifo_is_empty(fifo_ctrl));
#else
    if (fifo_is_empty(fifo_ctrl)) {
        printf("fifo is empty\n");
        return 0xdeadbeef;
    }
#endif
    uint64_t element = *(((uint64_t *) fifo_ctrl->data_array) + *(fifo_ctrl->head_ptr) * 2);
    __sync_synchronize;
    *fifo_ctrl->head_ptr = private_get_incremented_head(fifo_ctrl);
    return element;
}


void fifo_push_32(uint64_t element, fifo_ctrl_t* fifo_ctrl)
{
    // loop whilie the fifo is full
    while (fifo_is_full(fifo_ctrl));
    *(((uint32_t *) fifo_ctrl->data_array) + (*fifo_ctrl->tail_ptr * 2)) = (uint32_t) element;
    __sync_synchronize;
    *fifo_ctrl->tail_ptr = private_get_incremented_tail(fifo_ctrl);
}

uint64_t fifo_pop_32(fifo_ctrl_t *fifo_ctrl)
{
    while (fifo_is_empty(fifo_ctrl));
    uint32_t element = *(((uint32_t *) fifo_ctrl->data_array) + *fifo_ctrl->head_ptr);
    __sync_synchronize;
    *fifo_ctrl->head_ptr = private_get_incremented_head(fifo_ctrl);
    return element;
}

// philosophy: don't chunk requests in software for better transparency
// as 128 bits aren't supported, 64 would suffice
void fifo_push_16(uint64_t element, fifo_ctrl_t* fifo_ctrl)
{
    // loop whilie the fifo is full
    while (fifo_is_full(fifo_ctrl));
    *(((uint16_t *) fifo_ctrl->data_array) + *fifo_ctrl->tail_ptr) = (uint16_t) element;
    __sync_synchronize;
    *fifo_ctrl->tail_ptr = private_get_incremented_tail(fifo_ctrl);
}

uint64_t fifo_pop_16(fifo_ctrl_t *fifo_ctrl)
{
    while (fifo_is_empty(fifo_ctrl));
    uint16_t element = *(((uint16_t *) fifo_ctrl->data_array) + *fifo_ctrl->head_ptr);
    __sync_synchronize;
    *fifo_ctrl->head_ptr = private_get_incremented_head(fifo_ctrl);
    return element;
}

// philosophy: don't chunk requests in software for better transparency
// as 128 bits aren't supported, 64 would suffice
void fifo_push_8(uint64_t element, fifo_ctrl_t* fifo_ctrl)
{
    // loop whilie the fifo is full
    while (fifo_is_full(fifo_ctrl));
    *(((uint8_t *) fifo_ctrl->data_array) + *fifo_ctrl->tail_ptr) = (uint8_t) element;
    __sync_synchronize;
    *fifo_ctrl->tail_ptr = private_get_incremented_tail(fifo_ctrl);
}

uint64_t fifo_pop_8(fifo_ctrl_t *fifo_ctrl)
{
    while (fifo_is_empty(fifo_ctrl));
    uint8_t element = *(((uint8_t *) fifo_ctrl->data_array) + *fifo_ctrl->head_ptr);
    __sync_synchronize;
    *fifo_ctrl->head_ptr = private_get_incremented_head(fifo_ctrl);
    return element;
}

// note that we cannot operate at sub-byte level in c
// this is not how c works
// note also that el is bits
// we need a byte clog2

uint16_t clog2(uint16_t bitswidth)
{
    uint16_t el = bitswidth / 8;
    switch (el) {
        case 8:
            return 3;
        case 4:
            return 2;
        case 2:
            return 1;
        case 1:
            return 0;
    }
}


void baremetal_write(uint32_t tile, uint64_t addr, uint64_t value)
{
    PRINTBT
    // this is copied from dcpn.h
  uint32_t tileno = tile*2+1;
  uint64_t base_dream = 0xe100B00000ULL | ((tileno) << 28) | ((0) << 24); 
  uint64_t write_addr = (addr*8) | base_dream; 
#ifdef PRI_DEBUG
  printf("Target DREAM addr: %lx, write config data: %lx\n", write_addr, (uint64_t)value);
#endif
#ifdef BARE_METAL
  *(volatile uint64_t*)write_addr = value;
  __sync_synchronize;
#else
    // mmap /dev/mem so we can access the allocated memory
    int fd = open("/dev/mem", O_RDWR);
    uint64_t page_size = getpagesize();
    uint64_t offset_in_page = (unsigned) write_addr & (page_size - 1);
    if (fd == -1) {
        perror("Page Alloc FD open Error");
    }

    uint64_t mmap_start = write_addr;
    //fprintf(stdout, "Start address %p\n", mmap_start);

    uint64_t * mmapped = (uint64_t *)mmap(NULL, page_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, write_addr & ~(off_t)(page_size - 1));
    if (mmapped == (uint64_t *) -1) {
        perror("Page Alloc MMAP Error");
    }
    void* virt_addr = (char*)mmapped + offset_in_page;
    *(uint64_t *) virt_addr = value;
#ifdef PRI_DEBUG
    fprintf(stdout, "Page mmap-ed: address %lx\n", mmap_start);
    fprintf(stdout, "Trying to write to address %p\n", virt_addr);
#endif
    __sync_synchronize;
    munmap(mmapped, page_size);
    close(fd);
#endif
}


uint64_t uncached_read(uint32_t tile, uint64_t addr)
{
    PRINTBT
    // this is copied from dcpn.h
  uint32_t tileno = tile*2+1;
  uint64_t base_dream = 0xe100B00000ULL | ((tileno) << 28) | ((0) << 24); 
  uint64_t read_addr = (addr*8) | base_dream; 
  uint64_t read_val;
#ifdef PRI_DEBUG
  printf("read DREAM addr: %lx\n", read_addr);
#endif

#ifdef BARE_METAL
    read_val = *(volatile uint64_t*)read_addr;
    __sync_synchronize;
#else
    // mmap /dev/mem so we can access the allocated memory
    int fd = open("/dev/mem", O_RDWR);
    uint64_t page_size = getpagesize();
    uint64_t offset_in_page = (unsigned) read_addr & (page_size - 1);
    if (fd == -1) {
        perror("Page Alloc FD open Error");
    }

    uint64_t mmap_start = read_addr;
    //fprintf(stdout, "Start address %p\n", mmap_start);

    uint64_t * mmapped = (uint64_t *)mmap(NULL, page_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, read_addr & ~(off_t)(page_size - 1));
    if (mmapped == (uint64_t *) -1) {
        perror("Page Alloc MMAP Error");
    }
    void* virt_addr = (char*)mmapped + offset_in_page;
    read_val =  *(uint64_t *)virt_addr;
#ifdef PRI_DEBUG
    fprintf(stdout, "Page mmap-ed: address %lx\n", mmap_start);
    fprintf(stdout, "Trying to read from address %p\n", virt_addr);
#endif
    __sync_synchronize;
    munmap(mmapped, page_size);
    close(fd);
#endif
    return read_val;
}


void cohort_off()
{
    PRINTBT
    // turn off the monitor
    baremetal_write(0, 7, 0);
    __sync_synchronize;
#ifndef BARE_METAL
    // don't flush in bare metal, because some things can go wrong
    dec_flush_tlb(0);
#endif
}

void cohort_on()
{
    PRINTBT
    // turn on the monitor
    // don't lower reset, but turn on and clear the monitor
    baremetal_write(0, 7, 7);
    __sync_synchronize;
}

void cohort_stop_monitors()
{
    // stop counter, but keep reset low
    baremetal_write(0, 7, 1);
}

void cohort_print_monitors()
{
    for (int i=0;i< 35; i++) {
        printf("%lx,",uncached_read(0, i));
    }

}

#endif // COHORT_FIFO_PARAM_H
