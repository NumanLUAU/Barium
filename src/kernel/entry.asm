[bits 64]
extern kmain
global _start
global gdt_load
global tss_load
global get_cs
global get_ds
global get_ss
global get_tr
global get_lar
global get_lsl

section .text
extern __bss_start
extern __bss_end

_start:
    mov r8, rcx
    mov rdi, __bss_start
    mov rcx, __bss_end
    sub rcx, rdi
    xor al, al
    rep stosb

    mov rsp, stack_top
    mov rdi, r8
    sub rsp, 32
    call kmain
    
    hlt

gdt_load:
    lgdt [rdi]
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    pop rdi
    push 0x08
    push rdi
    retfq

tss_load:
    mov ax, 0x28
    ltr ax
    ret



get_cs:
    mov rax, cs
    ret

get_ds:
    mov rax, ds
    ret

get_ss:
    mov rax, ss
    ret

get_tr:
    str rax
    ret

get_lar:
    lar rax, rdi
    ret

get_lsl:
    lsl rax, rdi
    ret



section .bss
align 16
stack_bottom:
    resb 16384
stack_top:
