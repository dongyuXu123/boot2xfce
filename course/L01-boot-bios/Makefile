# [课程模板 · S0] 构建与验收（进度门控）
QEMU  ?= qemu-system-i386
NASM  ?= nasm

L := $(shell sed -n 's/^#define LESSON *//p' progress.h 2>/dev/null || echo 1)

B := build

all: $(B)/mbr.bin

$(B):
	mkdir -p $(B)

$(B)/mbr.bin: mbr.asm | $(B)
	$(NASM) -f bin $< -o $@

$(B)/disk.img: $(B)/mbr.bin | $(B)
	dd if=/dev/zero of=$@ bs=512 count=2880 status=none
	dd if=$< of=$@ bs=512 conv=notrunc status=none

test: $(B)/disk.img
	@rm -f $(B)/serial.log
	@timeout 15 $(QEMU) -drive if=floppy,format=raw,file=$(B)/disk.img \
		-boot order=a -display none -no-reboot \
		-serial file:$(B)/serial.log > /dev/null 2>&1 || true
	@ok=1; \
	grep -qF "S0: power-on OK" $(B)/serial.log || { echo "[FAIL] 缺少: S0: power-on OK"; ok=0; }; \
	if [ $(L) -ge 3 ]; then \
		grep -qF "AX=1234" $(B)/serial.log || { echo "[FAIL] 缺少: AX=1234（十六进制自检）"; ok=0; }; \
	fi; \
	[ $$ok = 1 ] && echo "[PASS] L$(L)" || { echo "---- 串口输出 ----"; cat $(B)/serial.log; exit 1; }

run: $(B)/disk.img
	$(QEMU) -drive if=floppy,format=raw,file=$< -boot order=a

clean:
	rm -rf $(B)

.PHONY: all test run clean
