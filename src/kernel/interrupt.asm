[bits 64]

extern interrupt_handler
global idt_load
global isr_stub_table

%macro isr_no_err_stub 1
isr_stub_%1:
    push qword 0
    push qword %1
    jmp isr_common_stub
%endmacro

%macro isr_err_stub 1
isr_stub_%1:
    push qword %1
    jmp isr_common_stub
%endmacro

isr_common_stub:
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rdi
    push rsi
    push rbp
    push rdx
    push rcx
    push rbx
    push rax

    test qword [rsp + 152], 3
    jz .no_swap
    swapgs
.no_swap:

    mov rdi, rsp
    call interrupt_handler
    mov rsp, rax 

    test qword [rsp + 152], 3
    jz .no_swap_back
    swapgs
.no_swap_back:

    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rbp
    pop rsi
    pop rdi
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
    add rsp, 16
    iretq

%assign i 0
%rep 32
    %if i == 8 || (i >= 10 && i <= 14) || i == 17 || i == 21 || i == 29 || i == 30
        isr_err_stub i
    %else
        isr_no_err_stub i
    %endif
%assign i i+1
%endrep

%assign i 32
%rep 16
    isr_no_err_stub i
%assign i i+1
%endrep

idt_load:
    lidt [rdi]
    ret

section .data
isr_stub_table:
%assign i 0
%rep 48
    dq isr_stub_%+i
%assign i i+1
%endrep
