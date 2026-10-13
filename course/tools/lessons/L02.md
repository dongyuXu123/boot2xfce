# L02 · COM1 串口驱动

**目标**：不靠 BIOS，直接对端口编程——这是内核与外界的第一条自己修的路。
**你亲手写**（`mbr.asm`，两个函数）：
1. `serial_init`：IER 关中断 → LCR|0x80 开 DLAB → DLL=1/DLM=0（115200）→
   LCR=0x03（8N1）→ FCR=0xC7（FIFO）→ MCR=0x0B。每个都是 `out dx,al`。
2. `serial_putc`：轮询 LSR(COM1+5) bit5（发送保持寄存器空），写 COM1。
   **必须 `push ax`/`pop ax` 保护 AX**——主循环还靠它工作。

**背景**：串口是 16550 UART，一组 8 位端口；QEMU 把 COM1 接到
`-serial file:...`，测试断言全靠它。轮询一位一位发，慢但够用、可控。

**验收**：`make test`（和 L01 相同断言——但这次串口那一路是你自己写的）。
**参考答案**：`stages/s0-mbr/mbr.asm`。
