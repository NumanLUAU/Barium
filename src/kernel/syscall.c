#include <barium/syscall.h>
#include <barium/console.h>
#include <barium/lib.h>
#include <barium/sched.h>

extern void syscall_entry();

void syscall_handler(uint64_t syscall_no, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    if (syscall_no == 1) {
        console_newline();
        console_print((char*)arg1);
        console_newline();
    } else if (syscall_no == 2) {
        sched_exit();
    } else {
        console_print("unknown syscall: ");
        console_print_hex(syscall_no);
        console_newline();
    }
}

void syscall_init() {
    b_wrmsr(0xC0000080, b_rdmsr(0xC0000080) | 1); 
    b_wrmsr(0xC0000081, 0x0010000800000000ULL); 
    b_wrmsr(0xC0000082, (uint64_t)syscall_entry);
    b_wrmsr(0xC0000084, 0x200); 
}
