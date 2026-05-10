# Barium

Barium is a minimal x86_64 UEFI kernel with some modern features.

### features
- **Scheduler**: multi-threaded with 32 priority levels and round-robin fairness
- **Memory**: dual-stack memory management
- **Shell**: minimal interactive shell
- **PS/2 Support**: Barium does not have xHCI or EHCI yet, so you'll have to test in QEMU or a PS/2 supported device.

### requirements
- `x86_64-elf-gcc` and `x86_64-elf-ld`
- `x86_64-w64-mingw32-gcc`
- `mtools` and `dosfstools`
- `make`

### building
see [BUILDING.md](BUILDING.md) [WIP] for a full step-by-step guide on how to set up your environment.

if you already have everything set up, just run:
```bash
make
```
this produces `barium.img`, a bootable fat32 disk image containing the uefi bootloader (`/EFI/BOOT/BOOTX64.EFI`) and the kernel (`/kernel.bin`)

### testing
you can run it in qemu:
```bash
qemu-system-x86_64 -bios /path/to/ovmf.fd -drive format=raw,file=barium.img
```
once in the shell, use `help` to see available commands 

If you want to test on real devices, you can flash Barium.img to your flash drive and put that in any computer that has UEFI support. Please keep in mind that if your device does not have a PS/2 chip, it will not work.