# L06 · 解析 multiboot 内存大小

**目标**：第一次从引导器手里接收硬件信息——内存有多大。
**你亲手写**（`kernel.c` 的 L06 TODO 区，~3 行）：
用给定的 `print`/`print_dec` 打出：
`S1: mem lower=<mem_lower>KB, upper=<mem_upper/1024>MB`
注意 `mem_upper` 的单位是 **KB**，换 MB 要除以 1024。

**背景**：信息结构 `flags` 的 bit0=1 表示 `mem_lower`/`mem_upper` 有效；
lower 是常规内存(<1MiB)，upper 是 1MiB 以上的扩展内存。QEMU 默认 128MiB。

**验收**：`make test` 断言 `S1: mem upper=` 出现（顺便重考 L04/L05）。
**参考答案**：`stages/s1-grub/kernel.c`。
