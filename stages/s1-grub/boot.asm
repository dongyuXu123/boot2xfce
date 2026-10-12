; ============================================================
; S1 —— 把引导权交给 GRUB（Multiboot 1 规范）
;
; S0 里我们自己站在 0x7C00；从 S1 起，加电后先跑 GRUB：
;   BIOS → GRUB 引导映像 → grub.cfg 的 `multiboot /boot/kernel.elf`
;   → GRUB 按 Multiboot 头加载内核到 1MiB，跳到入口，并把
;     eax=0x2BADB002（魔数）、ebx=信息结构指针 交给内核。
;
; Multiboot 头必须位于 ELF 前 8K 内、4 字节对齐：
;   magic=0x1BADB002, flags=0x3（按 4K 页对齐加载 + 传内存大小），
;   checksum = -(magic+flags)
; ============================================================
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
    mov esp, stack_top      ; 建自己的栈
    push ebx                ; 参数2: multiboot 信息结构物理地址
    push eax                ; 参数1: 魔数（应为 0x2BADB002）
    call kmain
.hang:
    hlt
    jmp .hang

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:
