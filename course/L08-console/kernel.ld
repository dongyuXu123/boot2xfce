/* 内核链接脚本：加载到物理 1MiB；.userblob 是嵌入的 ring3 演示程序 */
ENTRY(_start)
SECTIONS
{
    . = 1M;

    .text BLOCK(4K) : ALIGN(4K)
    {
        *(.multiboot)   /* 头必须在最前 8K 内 */
        *(.text)
    }
    .rodata BLOCK(4K) : ALIGN(4K) { *(.rodata)   }
    .data   BLOCK(4K) : ALIGN(4K) { *(.data)     }
    .bss    BLOCK(4K) : ALIGN(4K) { *(COMMON) *(.bss) }
    .userblob BLOCK(4K) : ALIGN(4K) { *(.userblob) }

    kernel_end = .;
}
