[bits 16]
[org 0x8000]

smp_trampoline:
    cli
    cld
    
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax

    lgdt [gdt32_ptr]

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp 0x08:smp_trampoline_32

[bits 32]
smp_trampoline_32:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax

    mov eax, [0x7000]
    mov cr3, eax

    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    lgdt [gdt64_ptr]

    jmp 0x08:smp_trampoline_64

[bits 64]
smp_trampoline_64:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax

    mov rax, 0x7008
    mov rsp, [rax]
    
    mov rax, 1
    mov rbx, 0x7018
    mov [rbx], rax

    mov rax, 0x7010
    mov rax, [rax]
    jmp rax

align 16
gdt32:
    dq 0 
    dq 0x00CF9A000000FFFF 
    dq 0x00CF92000000FFFF 
gdt32_ptr:
    dw $ - gdt32 - 1
    dd gdt32

align 16
gdt64:
    dq 0 
    dq 0x00AF9A000000FFFF 
    dq 0x00AF92000000FFFF 
gdt64_ptr:
    dw $ - gdt64 - 1
    dq gdt64

smp_trampoline_end:
