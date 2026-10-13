# [课程模板 · S2] 构建与验收（进度门控：progress.h 决定断言哪些标记）
CC    ?= gcc
LD    ?= ld
NASM  ?= nasm
QEMU  ?= qemu-system-i386

CFLAGS   := -m32 -ffreestanding -fno-pie -fno-stack-protector -fno-builtin \
            -nostdlib -nostdinc -I. -O2 -g -Wall -Wextra
ASFLAGS  := -f elf32
LDFLAGS  := -m elf_i386 -nostdlib -T kernel.ld --build-id=none
UCFLAGS  := -m32 -ffreestanding -fno-pie -fno-stack-protector -fno-builtin \
            -nostdlib -nostdinc -O1 -g -Wall -Wextra

KOBJS := build/boot.o build/cpu.o build/isr.o build/console.o build/gdt.o \
         build/idt.o build/irq.o build/mm.o build/task.o build/ramfs.o \
         build/syscall.o build/main.o build/blob.o

L := $(shell sed -n 's/^#define LESSON *//p' progress.h 2>/dev/null || echo 8)

B := build

all: $(B)/boot.iso

$(B):
	mkdir -p $(B)

build/%.o: %.c kernel.h progress.h | $(B)
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: %.asm | $(B)
	$(NASM) $(ASFLAGS) $< -o $@

# 用户程序：入口桩在镜像最前，静态链接 0x400000，平铺二进制
build/user.bin: user.c user_start.asm | $(B)
	$(NASM) $(ASFLAGS) user_start.asm -o $(B)/user_start.o
	$(CC) $(UCFLAGS) -c user.c -o $(B)/user.o
	$(LD) -m elf_i386 -nostdlib -static -no-pie -Ttext=0x400000 \
		--build-id=none -o $(B)/user.elf $(B)/user_start.o $(B)/user.o
	objcopy -O binary $(B)/user.elf $@

build/blob.o: blob.asm build/user.bin | $(B)
	$(NASM) $(ASFLAGS) $< -o $@

$(B)/kernel.elf: $(KOBJS) kernel.ld
	$(LD) $(LDFLAGS) -o $@ $(KOBJS)

$(B)/boot.iso: $(B)/kernel.elf iso/boot/grub/grub.cfg
	rm -rf $(B)/iso && mkdir -p $(B)/iso/boot/grub
	cp $(B)/kernel.elf $(B)/iso/boot/kernel.elf
	cp iso/boot/grub/grub.cfg $(B)/iso/boot/grub/grub.cfg
	grub-mkrescue -o $@ $(B)/iso > /dev/null 2>&1

# 验收标记表：第 N 课须通过前 N-7 课全部标记（回归内置）
TEST_RUN = timeout 25 $(QEMU) -cdrom $(B)/boot.iso -boot d -display none \
	-no-reboot -serial file:$(B)/serial.log > /dev/null 2>&1 || true

test: $(B)/boot.iso
	@rm -f $(B)/serial.log
	@if [ $(L) -ge 18 ]; then \
		( sleep 8; printf 'sendkey a\n'; sleep 2; printf 'quit\n' ) \
		  | $(QEMU) -cdrom $(B)/boot.iso -boot d -display none -no-reboot \
		    -serial file:$(B)/serial.log -monitor stdio > /dev/null 2>&1 || true; \
	else \
		$(TEST_RUN); \
	fi
	@ok=1; \
	for m in "S2: console OK"; do \
		grep -qF "$$m" $(B)/serial.log || { echo "[FAIL] 缺少: $$m"; ok=0; }; \
	done; \
	if [ $(L) -ge 9  ]; then grep -qF "S2: gdt OK"      $(B)/serial.log || { echo "[FAIL] 缺少: S2: gdt OK"; ok=0; }; fi; \
	if [ $(L) -ge 10 ]; then grep -qF "S2: idt OK"      $(B)/serial.log || { echo "[FAIL] 缺少: S2: idt OK"; ok=0; }; fi; \
	if [ $(L) -ge 11 ]; then grep -qF "S2: pic+pit OK"  $(B)/serial.log || { echo "[FAIL] 缺少: S2: pic+pit OK（ticks 没走？）"; ok=0; }; fi; \
	if [ $(L) -ge 12 ]; then grep -qF "S2: pmm OK"      $(B)/serial.log || { echo "[FAIL] 缺少: S2: pmm OK"; ok=0; }; fi; \
	if [ $(L) -ge 13 ]; then grep -qF "S2: heap OK"     $(B)/serial.log || { echo "[FAIL] 缺少: S2: heap OK"; ok=0; }; fi; \
	if [ $(L) -ge 14 ]; then grep -qF "taskA run 1"     $(B)/serial.log || { echo "[FAIL] 缺少: taskA run 1"; ok=0; }; \
	                     grep -qF "taskB run 1"     $(B)/serial.log || { echo "[FAIL] 缺少: taskB run 1"; ok=0; }; fi; \
	if [ $(L) -ge 15 ]; then grep -qF "S2: ramfs OK"    $(B)/serial.log || { echo "[FAIL] 缺少: S2: ramfs OK"; ok=0; }; fi; \
	if [ $(L) -ge 16 ]; then grep -qF "S2: syscall OK"  $(B)/serial.log || { echo "[FAIL] 缺少: S2: syscall OK"; ok=0; }; fi; \
	if [ $(L) -ge 17 ]; then grep -qF "[user] hello from ring3" $(B)/serial.log || { echo "[FAIL] 缺少: [user] hello from ring3"; ok=0; }; fi; \
	if [ $(L) -ge 18 ]; then grep -qF "kbd: a"          $(B)/serial.log || { echo "[FAIL] 缺少: kbd: a（注键没收到？）"; ok=0; }; fi; \
	if [ $(L) -ge 19 ]; then grep -qF "[capstone] writefile ret=13" $(B)/serial.log || { echo "[FAIL] 缺少: capstone writefile"; ok=0; }; \
	                     grep -qF "cap.txt(13B)"        $(B)/serial.log || { echo "[FAIL] 缺少: cap.txt(13B)（ls 里没有！）"; ok=0; }; fi; \
	[ $$ok = 1 ] && echo "[PASS] L$(L)" || { echo "---- 串口输出 ----"; cat $(B)/serial.log; exit 1; }

run: $(B)/boot.iso
	$(QEMU) -cdrom $< -boot d

clean:
	rm -rf $(B)

.PHONY: all test run clean
