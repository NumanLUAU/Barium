#include <barium/syscall.h>
#include <barium/lib.h>
#include <barium/console.h>
#include <barium/sched.h>

extern void syscall_entry();

void syscall_handler(uint64_t syscall_no, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    if (syscall_no == 1) {
        console_print("[user] ");
        console_print((const char*)arg1);
    } else if (syscall_no == 2) {
        sched_yield();
    } else if (syscall_no == 3) {
        console_print("[kernel] thread exited\n");
        sched_exit();
    }
}

void syscall_init() {
    uint64_t efer = b_rdmsr(0xC0000080);
    efer |= 1;
    b_wrmsr(0xC0000080, efer);
    uint64_t star = ((uint64_t)0x10 << 48) | ((uint64_t)0x08 << 32);
    b_wrmsr(0xC0000081, star);
    b_wrmsr(0xC0000082, (uint64_t)syscall_entry);
    b_wrmsr(0xC0000084, 0x200); 
}
