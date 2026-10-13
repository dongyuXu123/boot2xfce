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
         build/init_blob.o build/hello_blob.o build/sh_blob.o

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

test: $(B)/boot.iso
	@rm -f $(B)/serial.log
	@( sleep 10; for k in r u n spc h e l l o ret; do printf 'sendkey %s\n' $$k; sleep 0.4; done; sleep 5; printf 'quit\n' ) \
		| timeout 30 $(QEMU) -cdrom $< -boot d -display none -no-reboot \
		-serial file:$(B)/serial.log -monitor stdio > /dev/null 2>&1 || true
	@ok=1; \
	for m in "S3: GDT/IDT/PIC/PIT ready" "S3: pmm frames=" "S3: ramfs read-back OK" \
	         "taskA run 1" "taskB run 1" "S3: kernel core ready" \
	         "S3: demand page OK" "S3: per-task pd + ELF image OK" "[user] hello from ring3" "[child] fork=0, x=6" "[parent] child 5 reaped, x=5" "[hello] ELF exec OK" "S3: ramfs ls:" "[sh] shell ready" "[sh] child done"; do \
		grep -qF "$$m" $(B)/serial.log || { echo "[FAIL] s3 缺少标记: $$m"; ok=0; }; \
	done; \
	[ $$ok = 1 ] && echo "[PASS] s3" || { echo "---- 串口输出 ----"; cat $(B)/serial.log; exit 1; }

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
