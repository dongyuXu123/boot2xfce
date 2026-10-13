# L05 解答 —— 构建规则（课程模板 Makefile + iso 规则落解）
CC    ?= gcc
LD    ?= ld
NASM  ?= nasm
QEMU  ?= qemu-system-i386

CFLAGS   := -m32 -ffreestanding -fno-pie -fno-stack-protector -fno-builtin \
            -nostdlib -nostdinc -O2 -g -Wall -Wextra
ASFLAGS  := -f elf32
LDFLAGS  := -m elf_i386 -nostdlib -T kernel.ld --build-id=none

L := $(shell sed -n 's/^#define LESSON *//p' progress.h 2>/dev/null || echo 5)

B := build

all: $(B)/boot.iso

$(B):
	mkdir -p $(B)

$(B)/boot.o: boot.asm progress.h | $(B)
	$(NASM) $(ASFLAGS) $< -o $@

$(B)/kernel.o: kernel.c progress.h | $(B)
	$(CC) $(CFLAGS) -c $< -o $@

$(B)/kernel.elf: $(B)/boot.o $(B)/kernel.o kernel.ld | $(B)
	$(LD) $(LDFLAGS) -o $@ $(B)/boot.o $(B)/kernel.o

# L05 解答：ISO 制作四步——摆目录 → 拷内核 → 拷配置 → grub-mkrescue
$(B)/boot.iso: $(B)/kernel.elf iso/boot/grub/grub.cfg
	rm -rf $(B)/iso && mkdir -p $(B)/iso/boot/grub
	cp $(B)/kernel.elf $(B)/iso/boot/kernel.elf
	cp iso/boot/grub/grub.cfg $(B)/iso/boot/grub/grub.cfg
	grub-mkrescue -o $@ $(B)/iso > /dev/null 2>&1

test: $(B)/boot.iso
	@rm -f $(B)/serial.log
	@timeout 30 $(QEMU) -cdrom $< -boot d -display none -no-reboot \
		-serial file:$(B)/serial.log > /dev/null 2>&1 || true
	@ok=1; \
	for m in "S1: GRUB boot OK"; do \
		grep -qF "$$m" $(B)/serial.log || { echo "[FAIL] 缺少标记: $$m"; ok=0; }; \
	done; \
	[ $$ok = 1 ] && echo "[PASS] L$(L)" || { echo "---- 串口输出 ----"; cat $(B)/serial.log; exit 1; }

clean:
	rm -rf $(B)

.PHONY: all test clean
