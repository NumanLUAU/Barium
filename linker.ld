ENTRY(_start)

SECTIONS
{
    . = 0x100000;

    .text :
    {
        *(.text)
    }

    .data :
    {
        *(.data)
    }

    .bss :
    {
        __bss_start = .;
        *(.bss)
        *(COMMON)
        __bss_end = .;
    }
}
