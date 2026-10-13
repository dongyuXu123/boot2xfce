# L11 · PIC 重映射 + PIT 时钟

**目标**：让时钟中断第一次真正打进来——`ticks` 自己走起来。
**你亲手写**（`irq.c` 的 `pic_init` / `pit_init`）：
1. `pic_init`：主从 ICW1=0x11 → ICW2 偏移 0x20/0x28（错位=IRQ0 撞 8 号
   异常门，Double Fault 现场直播）→ ICW3 级联 0x04/0x02 → ICW4=0x01 →
   屏蔽字：主 0xF8（只放 IRQ0 时钟 / IRQ1 键盘 / IRQ2 级联），从 0xFF。
2. `pit_init(hz)`：除数 = 1193182/hz；`0x43←0x36`（通道0 方波 先低后高），
   `0x40←除数低/高字节`。

**验收（实测）**：kmain `sti()` 后等 `ticks>=3` 再打 `S2: pic+pit OK`——
桩是哑的，这个循环永远出不来，test 超时判红。
**参考答案**：`stages/s2-kernel-core/irq.c`。
