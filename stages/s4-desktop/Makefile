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
            -nostdlib -nostdinc -O1 -Wall -Wextra

KOBJS := build/boot.o build/cpu.o build/isr.o build/console.o build/gdt.o build/page.o \
         build/idt.o build/irq.o build/mm.o build/task.o build/ramfs.o \
         build/syscall.o build/main.o build/elf.o build/file.o build/tty.o \
         build/fb.o build/win.o build/mouse.o build/rtc.o \
         build/init_blob.o build/hello_blob.o build/sh_blob.o \
         build/calc_blob.o build/clock_blob.o build/notepad_blob.o

B := build

all: $(B)/boot.iso

$(B):
	mkdir -p $(B)

build/%.o: %.c kernel.h | $(B)
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: %.asm | $(B)
	$(NASM) $(ASFLAGS) $< -o $@

# 用户程序：独立 32 位 ELF（-static -no-pie，入口桩保证 _start 在最前）
build/init.elf: user/init.c user/ulib.h user/user_start.asm | $(B)
	$(NASM) $(ASFLAGS) user/user_start.asm -o $(B)/user_start.o
	$(CC) $(UCFLAGS) -c user/init.c -o $(B)/init.o
	$(LD) -m elf_i386 -nostdlib -static -no-pie -Ttext=0x400000 \
		-N --build-id=none -o $@ $(B)/user_start.o $(B)/init.o

build/hello.elf: user/hello.c user/ulib.h user/user_start.asm | $(B)
	$(CC) $(UCFLAGS) -c user/hello.c -o $(B)/hello.o
	$(LD) -m elf_i386 -nostdlib -static -no-pie -Ttext=0x400000 \
		-N --build-id=none -o $@ $(B)/user_start.o $(B)/hello.o

build/init_blob.o: init_blob.asm build/init.elf | $(B)
	$(NASM) $(ASFLAGS) $< -o $@

build/hello_blob.o: hello_blob.asm build/hello.elf | $(B)
	$(NASM) $(ASFLAGS) $< -o $@

build/sh.elf: user/sh.c user/ulib.h user/user_start.asm | $(B)
	$(CC) $(UCFLAGS) -c user/sh.c -o $(B)/sh.o
	$(LD) -m elf_i386 -nostdlib -static -no-pie -Ttext=0x400000 \
		-N --build-id=none -o $@ $(B)/user_start.o $(B)/sh.o

build/sh_blob.o: sh_blob.asm build/sh.elf | $(B)
	$(NASM) $(ASFLAGS) $< -o $@

build/calc.elf: user/calc.c user/ulib.h user/user_start.asm | $(B)
	$(NASM) $(ASFLAGS) user/user_start.asm -o $(B)/user_start.o
	$(CC) $(UCFLAGS) -c user/calc.c -o $(B)/calc.o
	$(LD) -m elf_i386 -nostdlib -static -no-pie -Ttext=0x400000 \
		-N --build-id=none -o $@ $(B)/user_start.o $(B)/calc.o

build/clock.elf: user/clock.c user/ulib.h user/user_start.asm | $(B)
	$(CC) $(UCFLAGS) -c user/clock.c -o $(B)/clock.o
	$(LD) -m elf_i386 -nostdlib -static -no-pie -Ttext=0x400000 \
		-N --build-id=none -o $@ $(B)/user_start.o $(B)/clock.o

build/notepad.elf: user/notepad.c user/ulib.h user/user_start.asm | $(B)
	$(CC) $(UCFLAGS) -c user/notepad.c -o $(B)/notepad.o
	$(LD) -m elf_i386 -nostdlib -static -no-pie -Ttext=0x400000 \
		-N --build-id=none -o $@ $(B)/user_start.o $(B)/notepad.o

build/calc_blob.o: calc_blob.asm build/calc.elf | $(B)
	$(NASM) $(ASFLAGS) $< -o $@

build/clock_blob.o: clock_blob.asm build/clock.elf | $(B)
	$(NASM) $(ASFLAGS) $< -o $@

build/notepad_blob.o: notepad_blob.asm build/notepad.elf | $(B)
	$(NASM) $(ASFLAGS) $< -o $@

test: $(B)/boot.iso
	@python3 ../../scripts/s4-test.py $(CURDIR)

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
