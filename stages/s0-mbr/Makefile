# S0 —— MBR 实模式引导（软盘映像 + QEMU 无头实测）
QEMU  ?= qemu-system-i386
NASM  ?= nasm

B := build

all: $(B)/mbr.bin

$(B):
	mkdir -p $(B)

$(B)/mbr.bin: mbr.asm | $(B)
	$(NASM) -f bin $< -o $@

$(B)/disk.img: $(B)/mbr.bin | $(B)
	dd if=/dev/zero of=$@ bs=512 count=2880 status=none
	dd if=$< of=$@ bs=512 conv=notrunc status=none

# 无头启动：串口落盘 → 断言启动横幅
test: $(B)/disk.img
	@rm -f $(B)/serial.log
	@timeout 15 $(QEMU) -drive if=floppy,format=raw,file=$(B)/disk.img \
		-boot order=a -display none -no-reboot \
		-serial file:$(B)/serial.log > /dev/null 2>&1 || true
	@grep -q "S0: power-on OK" $(B)/serial.log && echo "[PASS] s0" \
		|| { echo "[FAIL] s0"; cat $(B)/serial.log; exit 1; }

# 带窗口启动，亲眼看 VGA 输出（Ctrl+C 退出会杀掉 QEMU）
run: $(B)/disk.img
	$(QEMU) -drive if=floppy,format=raw,file=$< -boot order=a

clean:
	rm -rf $(B)

.PHONY: all test run clean
