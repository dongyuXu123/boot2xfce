; S2 与 S1 相同的 Multiboot 头。注意：GRUB2 的 multiboot1 加载器只认
; flags bit0(页对齐)/bit1(内存信息)/bit16(AOUT_KLUDGE)，多一位都会被
; "unsupported flag" 拒载；内存映射表不靠头里的 bit6 请求——GRUB 在
; PC 上总会把 E820 映射放进信息结构，内核侧看信息结构 flags bit6。
[BITS 32]

MBOOT_MAGIC    equ 0x1BADB002
MBOOT_FLAGS    equ 0x00000003
MBOOT_CHECKSUM equ -(MBOOT_MAGIC + MBOOT_FLAGS)

section .multiboot
align 4
    dd MBOOT_MAGIC
    dd MBOOT_FLAGS
    dd MBOOT_CHECKSUM

section .text
global _start
extern kmain

_start:
    cli
    mov esp, stack_top
    push ebx                        ; 参数2: multiboot 信息结构
    push eax                        ; 参数1: 魔数
    call kmain
.hang:
    hlt
    jmp .hang

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:
