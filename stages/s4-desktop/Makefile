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
	@rm -f $(B)/serial.log
	@( sleep 10; \
	   for k in r u n spc h e l l o ret; do printf 'sendkey %s\n' $$k; sleep 0.3; done; sleep 2; \
	   for r in 1 2 3; do for k in g f x ret; do printf 'sendkey %s\n' $$k; sleep 0.4; done; sleep 1; done; sleep 4; \
	   for k in c a l c ret; do printf 'sendkey %s\n' $$k; sleep 0.4; done; sleep 2; \
	   for k in 2 shift-equal 3 ret; do printf 'sendkey %s\n' $$k; sleep 0.6; done; sleep 2; \
	   printf 'sendkey esc\n'; sleep 2; \
	   for k in e c h o spc h i ret; do printf 'sendkey %s\n' $$k; sleep 0.6; done; sleep 2; \
	   for r in 1 2 3; do printf 'mouse_move 180 -120\nmouse_button 1\n'; sleep 0.5; printf 'mouse_button 0\n'; sleep 0.5; done; sleep 2; \
	   printf 'pmemsave 0xfd000000 1228800 "$(abspath $(B))/fb1.bin"\n'; sleep 1; \
	   printf 'pmemsave 0xfd000000 1228800 "$(abspath $(B))/fb2.bin"\n'; sleep 1; \
	   printf 'pmemsave 0xfd000000 1228800 "$(abspath $(B))/fb3.bin"\n'; sleep 1; printf 'quit\n' ) \
		| timeout 45 $(QEMU) -cdrom $< -boot d -display none -no-reboot -vga std \
		-serial file:$(B)/serial.log -monitor stdio > /dev/null 2>&1 || true
	@python3 ../../scripts/check-fb.py $(B)/fb1.bin $(B)/fb2.bin $(B)/fb3.bin
	@ok=1; \
	
	for m in "S4: GDT/IDT/PIC/PIT ready" "S4: pmm frames=" "S4: ramfs read-back OK" \
	         "taskA run 1" "taskB run 1" "S4: kernel core ready" \
	         "S4: demand page OK" "S4: per-task pd + ELF image OK" "[user] hello from ring3" \
	         "[child] fork=0, x=" "[parent] child 6 reaped, x=5" "[hello] ELF exec OK" \
	         "S4: ls " "[sh] shell ready" "[sh] child done" "S4: LFB 640x480x32" "[win] shell" "[click at"; do \
		grep -qF "$$m" $(B)/serial.log || { echo "[FAIL] s4 缺少标记: $$m"; ok=0; }; \
	done; \
	[ $$ok = 1 ] && echo "[PASS] s4" || { echo "---- 串口输出 ----"; cat $(B)/serial.log; exit 1; }

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
