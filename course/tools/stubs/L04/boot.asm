; ================================================================
; L04 桩 —— Multiboot 头与链接脚本（从 MBR 时代切到 ELF 内核）
; kernel.c 已给定（会打印 S1: GRUB boot OK），你负责让 GRUB 认识它：
;   1) boot.asm: 合法的 Multiboot 1 头（三件套 + 栈 + 进 kmain）
;   2) kernel.ld: 把内核放到物理 1MiB 的链接脚本
; ================================================================
[BITS 32]

; TODO(你来写·L04): Multiboot 1 头三件套（必须 4 字节对齐、位于文件前 8K）
;   magic     = 0x1BADB002
;   flags     = 0x3           （页对齐 + 内存信息。多一位 GRUB 都拒载！）
;   checksum  = -(magic+flags)
; 然后是 32 位入口 _start：关中断 → 设栈(stack_top) → push ebx(信息结构)
;   → push eax(魔数) → call kmain → hlt 死循环
;   栈区放 .bss：resb 16384
section .multiboot
align 4
    dd 0                        ; TODO: 替换我（magic）
    dd 0                        ; TODO: 替换我（flags）
    dd 0                        ; TODO: 替换我（checksum）

section .text
global _start
extern kmain

_start:
    hlt
    jmp _start                  ; TODO: 替换我（建栈、传参、调 kmain）

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:
