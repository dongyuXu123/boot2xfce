# L01 · 引导扇区与 BIOS 输出

**目标**：机器加电后，让属于你的第一段代码在屏幕上说话。
**你亲手写**（`mbr.asm`，只有两处 TODO）：
1. 打印循环：`si` 指向字符串，`lodsb` 取字符，`or al,al` 判结尾，
   BIOS `int 10h`(AH=0Eh, BX=0007h) 打印，再送串口，循环直到 0。
2. `msg` 字符串：内容必须是
   `S0: power-on OK - BIOS loaded MBR at 0x7C00, real mode alive`（`\r\n` 结尾）。
3. 扇区尾部：补零到 510 字节 + `dw 0xAA55` 引导签名——没有它 BIOS 不认。

**背景**：BIOS POST 完把第一个扇区搬到 `0x7C00` 跳过去；此时是 16 位实模式，
一切都靠中断和端口。串口函数是给定设施（L02 你会亲手重写它）。

**验收**：`make test`（QEMU 软盘引导，断言串口日志出现 `S0: power-on OK`）。
**试试作死**：删掉 `0xAA55` 再 test，感受 BIOS 的沉默。
**参考答案**：`stages/s0-mbr/mbr.asm`；原理：`docs/00-poweron.md`。
