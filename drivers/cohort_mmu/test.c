/*
    Test application for the Cohort MMU Driver
*/

#include <linux/syscalls.h>
#include <asm/unistd.h>

int main(){

    print("Cohort MMU Driver test application entered!\n");

    unsigned int *base_ptr;

    int ret = syscall(258, base_ptr);

    print("Syscall result for the mmu_notifier register is: %d\n", ret);
    print("Bse pointer addr for the mmu_notifier register is: %x\n", *base_ptr);

    // to be continued for testing tlb_flush and page_fault :)
    
    return 0;
}