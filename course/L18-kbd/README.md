# L18 · 键盘 IRQ1（真·硬验收）

**目标**：敲键有回应——而且这次验收是 QEMU 真的替你敲了一个键。
**你亲手写**（`irq.c` 的 `kbd_intr`）：
1. `inb(0x60)` 读通码（这一读同时是对键盘控制器的应答，别省）
2. 最高位=1 是释放码，忽略
3. 查 `sc2ascii` 表（越界或表值为 0 跳过）
4. 环形缓冲：判满用 `next != tail`（想想为什么不能用 head==tail，
   那样满和空就分不开了）；入队 `kbuf[head]=c; head=(head+1)%32`

**验收（硬核）**：`make test` 对 L≥18 会用 QEMU monitor 的 `sendkey a`
真的注入一次按键，断言串口出现 `kbd: a`（kbd 任务从缓冲取出并回显）。
桩不读端口 → 注键石沉大海 → 必红。
**参考答案**：`stages/s2-kernel-core/irq.c`。
