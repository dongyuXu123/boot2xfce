/* L04 桩 —— 内核链接脚本
 * 目标布局：内核加载到物理 1MiB（Multiboot 约定），四个段各 4K 对齐，
 * 末尾导出 kernel_end 符号（以后 PMM 保留区要用）。
 * 大坑：.multiboot 输入节必须放在输出最前面（头要在前 8K 内）。 */
ENTRY(_start)
SECTIONS
{
    . = 1M;

    /* TODO(你来写·L04): 补全四个输出节
     * .text    : { *(.multiboot) *(.text) }
     * .rodata  : { *(.rodata) }
     * .data    : { *(.data) }
     * .bss     : { *(COMMON) *(.bss) }
     * 每个都要 BLOCK(4K) : ALIGN(4K)，最后 kernel_end = .; */
}
