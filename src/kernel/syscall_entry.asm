[bits 64]

extern syscall_handler
global syscall_entry

syscall_entry:
    swapgs
    mov [gs:0x10], rsp
    mov rsp, [gs:0x00]
    
    push rcx
    push r11
    
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15
    
    mov r9, r8
    mov r8, r10
    mov rcx, rdx
    mov rdx, rsi
    mov rsi, rdi
    mov rdi, rax
    
    call syscall_handler
    
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    
    pop r11
    pop rcx
    
    mov rsp, [gs:0x10]
    swapgs
    
    sysretq
