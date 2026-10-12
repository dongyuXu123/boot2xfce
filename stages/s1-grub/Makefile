# S1 —— GRUB(Multiboot) 引导最小内核，grub-mkrescue 制 ISO + QEMU 无头实测
CC    ?= gcc
LD    ?= ld
NASM  ?= nasm
QEMU  ?= qemu-system-i386

CFLAGS   := -m32 -ffreestanding -fno-pie -fno-stack-protector -fno-builtin \
            -nostdlib -nostdinc -O2 -g -Wall -Wextra
ASFLAGS  := -f elf32
LDFLAGS  := -m elf_i386 -nostdlib -T kernel.ld --build-id=none

B := build

all: $(B)/boot.iso

$(B):
	mkdir -p $(B)

$(B)/boot.o: boot.asm | $(B)
	$(NASM) $(ASFLAGS) $< -o $@

$(B)/kernel.o: kernel.c | $(B)
	$(CC) $(CFLAGS) -c $< -o $@

$(B)/kernel.elf: $(B)/boot.o $(B)/kernel.o kernel.ld | $(B)
	$(LD) $(LDFLAGS) -o $@ $(B)/boot.o $(B)/kernel.o

$(B)/boot.iso: $(B)/kernel.elf iso/boot/grub/grub.cfg
	rm -rf $(B)/iso && mkdir -p $(B)/iso/boot/grub
	cp $(B)/kernel.elf $(B)/iso/boot/kernel.elf
	cp iso/boot/grub/grub.cfg $(B)/iso/boot/grub/grub.cfg
	grub-mkrescue -o $@ $(B)/iso > /dev/null 2>&1

test: $(B)/boot.iso
	@rm -f $(B)/serial.log
	@timeout 30 $(QEMU) -cdrom $< -boot d -display none -no-reboot \
		-serial file:$(B)/serial.log > /dev/null 2>&1 || true
	@grep -q "S1: GRUB boot OK" $(B)/serial.log \
		&& grep -q "S1: stage1 done" $(B)/serial.log && echo "[PASS] s1" \
		|| { echo "[FAIL] s1"; cat $(B)/serial.log; exit 1; }

run: $(B)/boot.iso
	$(QEMU) -cdrom $< -boot d

clean:
	rm -rf $(B)

.PHONY: all test run clean
