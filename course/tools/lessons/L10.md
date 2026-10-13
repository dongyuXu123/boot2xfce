# L10 · IDT 与中断入口桩表

**目标**：把 256 个中断门钉好——从这课起，硬件才能真正"打断"我们。
**你亲手写**：
1. `idt.c` 的 `idt_init`：清零 → 0..47 号门挂 `isr_table[i]`（flags=0x8E）→
   0x80 门挂 `isr_table[48]`（flags=**0xEE**，DPL=3 让用户态能 int）→
   填 `idtp` → `lidt`。
2. `isr.asm`（重头戏）：49 个入口 + 公共出口。
   - 无错误码异常/IRQ/int80：`PUSHALL`（pusha + 4 个段寄存器）→
     `push <号>` → 跳公共出口
   - 带错误码异常（8,10-14,17,21,29,30）：**先 `pop` 掉 CPU 压的错误码**
     存进全局 `isr_err_code`，再统一栈型——不然 iret 时栈对不齐直接炸
   - 公共出口：`push esp`（正指向 int_no = regs*）→ `call isr_dispatch` →
     `add esp,4` ×2 → `POPALL` → `iretd`
   栈布局必须与 `kernel.h` 的 `struct regs` 逐字段一致。

**验收**：kmain 用 `sidt` 检查 limit==2047 且基址落在内核镜像区。
**参考答案**：`stages/s2-kernel-core/{idt.c,isr.asm}`。
