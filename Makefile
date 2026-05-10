CC_BOOT = x86_64-w64-mingw32-gcc
CC_KERN = /home/pixel/barium-toolchain/bin/x86_64-elf-gcc
LD_KERN = /home/pixel/barium-toolchain/bin/x86_64-elf-ld
NASM = nasm

CFLAGS_BOOT = -ffreestanding -fshort-wchar -Iinclude -Wall -Wextra
CFLAGS_KERN = -ffreestanding -mcmodel=large -mno-red-zone -mgeneral-regs-only -fno-stack-protector -fno-stack-check -nostdlib -Iinclude -Wall -Wextra -g

LDFLAGS_KERN = -T linker.ld -nostdlib -z max-page-size=0x1000

BOOT_SRCS = src/boot/main.c
BOOT_EFI = bin/BOOTX64.EFI

KERN_SRCS = src/kernel/kernel.c \
            src/kernel/gdt.c \
            src/kernel/idt.c \
            src/kernel/vmm.c \
            src/kernel/heap.c \
            src/kernel/apic.c \
            src/kernel/console.c \
            src/kernel/keyboard.c \
            src/kernel/lib.c \
            src/kernel/shell.c \
            src/kernel/pmm.c \
            src/kernel/sched.c \
            src/kernel/cpu.c \
            src/kernel/syscall.c
KERN_OBJS = build/entry.o \
            build/interrupt.o \
            build/syscall_entry.o \
            $(KERN_SRCS:src/kernel/%.c=build/%.o)
KERN_BIN = bin/kernel.bin

IMG = barium.img

all:
	$(MAKE) clean
	$(MAKE) $(IMG)

$(BOOT_EFI): $(BOOT_SRCS)
	@mkdir -p bin
	$(CC_BOOT) $(CFLAGS_BOOT) -shared -Wl,-dll -Wl,--subsystem,10 -e efi_main -o $@ $<

build/entry.o: src/kernel/entry.asm
	@mkdir -p build
	$(NASM) -f elf64 $< -o $@

build/interrupt.o: src/kernel/interrupt.asm
	@mkdir -p build
	$(NASM) -f elf64 $< -o $@

build/syscall_entry.o: src/kernel/syscall_entry.asm
	@mkdir -p build
	$(NASM) -f elf64 $< -o $@

build/%.o: src/kernel/%.c
	@mkdir -p build
	$(CC_KERN) $(CFLAGS_KERN) -c -o $@ $<

KERN_ELF = bin/kernel.elf

$(KERN_BIN): $(KERN_OBJS)
	@mkdir -p bin
	$(LD_KERN) $(LDFLAGS_KERN) -o $(KERN_ELF) $(KERN_OBJS)
	/home/pixel/barium-toolchain/bin/x86_64-elf-objcopy -O binary $(KERN_ELF) $@

$(IMG): $(BOOT_EFI) $(KERN_BIN)
	dd if=/dev/zero of=$(IMG) bs=1M count=64
	mkfs.vfat -F 32 $(IMG)
	mmd -i $(IMG) ::/EFI
	mmd -i $(IMG) ::/EFI/BOOT
	mcopy -i $(IMG) $(BOOT_EFI) ::/EFI/BOOT/BOOTX64.EFI
	mcopy -i $(IMG) $(KERN_BIN) ::/kernel.bin

clean:
	rm -rf build bin $(IMG)

run-img: $(IMG)
	qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -drive file=$(IMG),if=ide,format=raw -net none
