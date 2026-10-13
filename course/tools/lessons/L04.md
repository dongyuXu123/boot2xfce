# L04 · Multiboot 头与链接脚本

**目标**：从"自己站在 0x7C00"升级为"按 Multiboot 协议被 GRUB 加载到 1MiB"。
**你亲手写**：
1. `boot.asm` 的 Multiboot 头三件套：`magic=0x1BADB002`、`flags=0x3`、
   `checksum=-(magic+flags)`——4 字节对齐、位于前 8K。
   ⚠️ flags 只能是 0x3！实测 GRUB2 对 bit6 都会 `unsupported flag: 0x43` 拒载。
2. `_start`：cli → `mov esp, stack_top` → `push ebx`（信息结构）→
   `push eax`（魔数）→ `call kmain` → hlt 死循环。
3. `kernel.ld`：`. = 1M;` 起，text/rodata/data/bss 各 BLOCK(4K):ALIGN(4K)，
   `.text` 里 `*(.multiboot)` 放最前，结尾 `kernel_end = .;`。

**背景**：GRUB 校验 checksum 后把 ELF LOAD 段搬到链接地址，跳到入口，
`eax`=魔数(0x2BADB002)、`ebx`=信息结构指针。cdecl 传参 = 后声明的先压。

**验收**：`make test` 断言串口出现 `S1: GRUB boot OK`（kernel.c 是给定的）。
**参考答案**：`stages/s1-grub/`；坑位详解：`docs/10-grub.md`。
