[bits 64]
global syscall_entry
extern syscall_handler

syscall_entry:
    swapgs
    mov [gs:8], rsp
    mov rsp, [gs:0] 
    
    push r11 
    push rcx 
    
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15
    
    push rdi
    push rsi
    push rdx

    mov rcx, rdx 
    mov rdx, rsi
    mov rsi, rdi
    mov rdi, rax
    
    call syscall_handler

    pop rdx
    pop rsi
    pop rdi

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp

    pop rcx
    pop r11

    ; sysretq alternative using iretq
    push 0x1B ; SS
    mov rax, [gs:8]
    push rax  ; RSP
    push r11  ; RFLAGS
    push 0x23 ; CS
    push rcx  ; RIP
    
    swapgs
    iretq
