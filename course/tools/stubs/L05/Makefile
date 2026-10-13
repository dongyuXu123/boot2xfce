# L05 桩 —— 构建规则：只有 iso 规则是你的作业，其余照旧
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

# TODO(你来写·L05): 补全 ISO 制作
# 步骤：
#   1) rm -rf $(B)/iso 并 mkdir -p $(B)/iso/boot/grub
#   2) cp $(B)/kernel.elf 到 $(B)/iso/boot/kernel.elf
#   3) cp iso/boot/grub/grub.cfg 到 $(B)/iso/boot/grub/
#   4) grub-mkrescue -o $@ $(B)/iso
$(B)/boot.iso: $(B)/kernel.elf iso/boot/grub/grub.cfg
	echo "TODO(L05): 亲手写这个规则（见任务书）" && false

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
