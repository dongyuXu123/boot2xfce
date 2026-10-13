# L09 · GDT 与 TSS 描述符

**目标**：告诉 CPU"段"的规则——平坦模型下它更像仪式，但 ring3（L17）
全靠它区分内核和用户。
**你亲手写**（`gdt.c` 的 `gdt_init`）：
按桩里注释的表填 6 个描述符（用给定的 `gdt_set`）：
null / 0x08 内核码(0x9A) / 0x10 内核数据(0x92) / 0x18 用户码(0xFA) /
0x20 用户数据(0xF2) / 0x28 TSS(base=&tss, access=0x89)。
然后填 `gp`（limit=sizeof(gdt)-1, base=(uint32_t)gdt）→
`gdt_flush((uint32_t)&gp)`（重载全部段寄存器）→ `tss_flush()`（ltr）→
`tss_set_esp0(0)`。
**写注释回答自己**：access 里的 9/F 高 4 位各是什么？0xCF 是什么？

**验收（硬件自检）**：kmain 用 `sgdt` 检查 limit 必须等于 6*8-1=47——
桩不做 lgdt，读到的还是 GRUB 的 GDT，必红。
**参考答案**：`stages/s2-kernel-core/gdt.c`。
