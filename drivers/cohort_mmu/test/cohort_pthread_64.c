#include <stdio.h>
#include "util.h"
#include "cohort_fifo.h"
#include <time.h>

#include <pthread.h>

#ifndef PRODUCER_FIFO_LENGTH 
#define PRODUCER_FIFO_LENGTH 32
#endif

#ifndef CONSUMER_FIFO_LENGTH 
#define CONSUMER_FIFO_LENGTH 32
#endif

#ifndef WAIT_COUNTER_VAL 
#define WAIT_COUNTER_VAL 1024
#endif

#ifndef SERIALIZATION_VAL
#define SERIALIZATION_VAL 1024
#endif

#ifndef DESERIALIZATION_VAL
#define DESERIALIZATION_VAL 1
#endif

#ifndef BACKOFF_COUNTER_VAL
#define BACKOFF_COUNTER_VAL 0x800
#endif

#define NUM_WORDS 16
static uint64_t A,D;
static uint32_t Dp[2] = {0x000000FF,0x000000AA};
static uint32_t Ap[NUM_WORDS] = {0x33221100,
                                                 0x77665544,
                                                 0xBBAA9988,
                                                 0xFFEEDDCC,
                                                 0x11111111,
                                                 0x22222222,
                                                 0x33333333,
                                                 0x44444444,
                                                 0x55555555,
                                                 0x66666666,
                                                 0x77777777,
                                                 0x88888888,
                                                 0x99999999,
                                                 0xAAAAAAAA,
                                                 0xBBBBBBBB,
                                                 0xCCCCCCCC};


#ifndef NUM_A
    #define NUM_A 1
#endif

void _kernel_(uint32_t id, uint32_t core_num){
    dec_open_producer(id);
    dec_open_consumer(id);
    // Enable Virtual Addr Translation
    dec_set_tlb_ptbase(0,0);

    A = (uint64_t)Ap | 0x7F00000000LL;
    D = (uint64_t)Dp | 0x7F00000000LL;

    uint64_t ppn = ((uint64_t)Ap >> 12);
    uint64_t vpn = ((uint64_t)A >> 12);
    print64("virtual page number", vpn);
    uint64_t mmpage = 0x0LL;
    mmpage |= ppn << 4; // [31:4] PPN
    mmpage |= vpn << 32; // [58:32] VPN
    // [63:62] is page size (1G,2M), flag bits are all 1's
    print64("Set TLB entry",mmpage);
    dec_set_tlb_mmpage(0, mmpage);

    //print64("Get TLB fault",dec_get_tlb_fault(0) );
    for (int i=0;i<1;i++){
        // Lower 4 bits {dirty bit, is_2M,is_1G,valid}
        // [31:4] is PPN
        // [59:32] is Asid, VPN
        // [63:60] Flag bits
        //print64("Get TLB entry",dec_snoop_tlb_entry(0) );
    }

    dec_set_base32(id,(void *)A);
    for (int i=0;i<4;i++){
        //dec_produce32(id,42);
        //dec_load32_asynci_llc(id,i%2);
        dec_atomic_fetch_add_asynci(id,i%2,1);
        __sync_synchronize;
        print32("A +1",dec_consume32(id));

        dec_set_base32(id,(void *)D);
        dec_atomic_compare_exchange_asynci(id,i%2, Dp[i%2], Dp[i%2]+1);
        print32("D CAS(+1)",dec_consume32(id));

        dec_set_base32(id,(void *)A);
        dec_atomic_compare_exchange_asynci(id,i%2, Ap[i%2], Ap[i%2]+1);
        print32("A CAS(+1)",dec_consume32(id));

        dec_load32_asynci(id,i%2);
        print32("TLoad A",dec_consume32(id));

        //__sync_synchronize; FENCE;
        //if (data!=A[0]) return 1;
    }
}

int cohort_set_tlb(uint64_t vpn, uint64_t ppn)
{
//    print64("virtual page number", vpn);
//    print64("physical page number", ppn);
    uint64_t mmpage = 0x0ULL;
    mmpage |= ppn << 4; // [31:4] PPN
    mmpage |= vpn << 32; // [58:32] VPN
    // [63:62] is page size (1G,2M), flag bits are all 1's
//    print64("Set TLB entry",mmpage);
    dec_set_tlb_mmpage(0, mmpage);
//    print64("Get TLB entry",dec_snoop_tlb_entry(0) );
}

void *producer_func(void *arg) {
    unsigned long long int write_value = 11;
    unsigned long long int serialization_value = SERIALIZATION_VAL;
    unsigned long long int deserialization_value = DESERIALIZATION_VAL;
    unsigned long long int wait_counter = WAIT_COUNTER_VAL;
    unsigned long long int backoff_counter = 0x800;

    write_value |= backoff_counter << 48;
    write_value |= serialization_value << 32;
    write_value |= deserialization_value << 16;
    write_value |= wait_counter << 4;
    __sync_synchronize;
    
    // start the count
    baremetal_write(0, 7, write_value);

    fifo_ctrl_t *sw_to_cohort_fifo = (fifo_ctrl_t *) arg;
    for (int i = 0; i < PRODUCER_FIFO_LENGTH; i++) {
        sw_to_cohort_fifo->fifo_push_func(i, sw_to_cohort_fifo);
    }
    cohort_stop_monitors();
}

int main(int argc, char ** argv) {
    volatile static uint32_t amo_cnt = 0;
    uint32_t id, core_num;
    id = 0;
    core_num = 1;
    // only make the first ariane initialize the tile
    if (id == 0) init_tile(NUM_A);

    if (PRODUCER_FIFO_LENGTH < CONSUMER_FIFO_LENGTH ) {
        printf("trying to consume more than produced, exiting\n");
        exit(-1);
    }

    
    // 32 bits elements, fifo length = 8
    fifo_ctrl_t *sw_to_cohort_fifo = fifo_init( 512 + 32, 64, 0);
    fifo_ctrl_t *cohort_to_sw_fifo = fifo_init( 512 + 32, 64, 1);
    void *acc_address = memalign(128, 128);
    memset(acc_address, 0, 128);

    baremetal_write(0, 6, (uint64_t) acc_address);

    pthread_t thread_id;

    // clear counter and turn on the monitor
    cohort_on();
    clock_t start, end;
    double cpu_time_used;

    unsigned long long int write_value = 11;
    unsigned long long int serialization_value = SERIALIZATION_VAL;
    unsigned long long int deserialization_value = DESERIALIZATION_VAL;
    unsigned long long int wait_counter = WAIT_COUNTER_VAL;
    unsigned long long int backoff_counter = 0x800;

    write_value |= backoff_counter << 48;
    write_value |= serialization_value << 32;
    write_value |= deserialization_value << 16;
    write_value |= wait_counter << 4;
    __sync_synchronize;
    
    sleep(4);
    
    // start the count
    baremetal_write(0, 7, write_value);
    start = clock();

    int err = pthread_create(&thread_id, NULL, producer_func, (void *) sw_to_cohort_fifo);
   
    for (int i = 0; i < CONSUMER_FIFO_LENGTH ; i++) {
        cohort_to_sw_fifo->fifo_pop_func(cohort_to_sw_fifo);
    }

    err = pthread_join(thread_id, NULL);

    end = clock();
    
    cohort_print_monitors();
    printf("%d, %d, %d, %d, %d, %d, ", PRODUCER_FIFO_LENGTH, CONSUMER_FIFO_LENGTH, BACKOFF_COUNTER_VAL, SERIALIZATION_VAL, DESERIALIZATION_VAL, WAIT_COUNTER_VAL );
    printf("%llx, %llx, time %f\n", end, start ,(double)(end - start) / CLOCKS_PER_SEC);
   
    cohort_off();
    fifo_deinit(sw_to_cohort_fifo);
    fifo_deinit(cohort_to_sw_fifo);
    free(acc_address);

#ifdef BARE_METAL
    if (ret == 7) {
        pass();
    } else {
        fail();
    }
#endif
    return 0;
}