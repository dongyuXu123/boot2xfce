# S2 —— 内核基础框架：mm + task/sched + ramfs + syscall(+ring3)
# 用户程序独立编译成平铺二进制，经 incbin 嵌入内核，启动后拷到 USER_BASE。
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

B := build

all: $(B)/boot.iso

$(B):
	mkdir -p $(B)

build/%.o: %.c kernel.h | $(B)
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: %.asm | $(B)
	$(NASM) $(ASFLAGS) $< -o $@

# 用户程序：入口桩在镜像最前，静态链接到 0x400000，objcopy 出平铺二进制
build/user.bin: user.c user_start.asm | $(B)
	$(NASM) $(ASFLAGS) user_start.asm -o $(B)/user_start.o
	$(CC) $(UCFLAGS) -c user.c -o $(B)/user.o
	$(LD) -m elf_i386 -nostdlib -static -no-pie -Ttext=0x400000 \
		--build-id=none -o $(B)/user.elf $(B)/user_start.o $(B)/user.o
	objcopy -O binary $(B)/user.elf $@

build/blob.o: blob.asm build/user.bin | $(B)

test: $(B)/boot.iso
	@rm -f $(B)/serial.log
	@timeout 20 $(QEMU) -cdrom $< -boot d -display none -no-reboot \
		-serial file:$(B)/serial.log > /dev/null 2>&1 || true
	@ok=1; \
	for m in "S2: GDT/IDT/PIC/PIT ready" "S2: pmm frames=" "S2: ramfs read-back OK" \
	         "taskA run 1" "taskB run 1" "S2: kernel core ready" \
	         "[user] hello from ring3" "S2: ramfs ls:"; do \
		grep -qF "$$m" $(B)/serial.log || { echo "[FAIL] s2 缺少标记: $$m"; ok=0; }; \
	done; \
	[ $$ok = 1 ] && echo "[PASS] s2" || { echo "---- 串口输出 ----"; cat $(B)/serial.log; exit 1; }

$(B)/boot.iso: $(B)/kernel.elf iso/boot/grub/grub.cfg
	rm -rf $(B)/iso && mkdir -p $(B)/iso/boot/grub
	cp $(B)/kernel.elf $(B)/iso/boot/kernel.elf
	cp iso/boot/grub/grub.cfg $(B)/iso/boot/grub/grub.cfg
	grub-mkrescue -o $@ $(B)/iso > /dev/null 2>&1

$(B)/kernel.elf: $(KOBJS) kernel.ld
	$(LD) $(LDFLAGS) -o $@ $(KOBJS)

run: $(B)/boot.iso
	$(QEMU) -cdrom $< -boot d

clean:
	rm -rf $(B)

.PHONY: all test run clean
