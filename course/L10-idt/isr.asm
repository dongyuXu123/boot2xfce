; ============================================================
; isr.asm —— 中断/异常入口桩
;
; 栈布局约定（与 kernel.h 的 struct regs 一致，低→高）：
;   int_no | gs fs es ds | edi..eax | (异常错误码已被提前弹走) | eip cs eflags [esp ss]
; 带错误码的异常（8,10-14,17,21,29,30）先 pop 到 isr_err_code，
; 使所有入口栈布局一致；错误码内容通过全局变量交给 C。
; ============================================================
[bits 32]

extern isr_dispatch
extern isr_err_code

%macro PUSHALL 0
    pusha
    push ds
    push es
    push fs
    push gs
%endmacro

%macro POPALL 0
    pop gs
    pop fs
    pop es
    pop ds
    popa
%endmacro

section .text

; ---- 无错误码的异常 / IRQ / 系统调用 ----
%macro STUB_NOERR 1
global stub_%1
stub_%1:
    PUSHALL
    push dword %1
    jmp isr_common
%endmacro

; ---- 带错误码的异常：先把 CPU 压的错误码弹进全局变量 ----
%macro STUB_ERR 1
global stub_%1
stub_%1:
    pop dword [isr_err_code]
    PUSHALL
    push dword %1
    jmp isr_common
%endmacro

STUB_NOERR  0
STUB_NOERR  1
STUB_NOERR  2
STUB_NOERR  3
STUB_NOERR  4
STUB_NOERR  5
STUB_NOERR  6
STUB_NOERR  7
STUB_ERR    8
STUB_NOERR  9
STUB_ERR   10
STUB_ERR   11
STUB_ERR   12
STUB_ERR   13
STUB_ERR   14
STUB_NOERR 15
STUB_NOERR 16
STUB_ERR   17
STUB_NOERR 18
STUB_NOERR 19
STUB_NOERR 20
STUB_ERR   21
STUB_NOERR 22
STUB_NOERR 23
STUB_NOERR 24
STUB_NOERR 25
STUB_NOERR 26
STUB_NOERR 27
STUB_NOERR 28
STUB_ERR   29
STUB_ERR   30
STUB_NOERR 31

%assign i 32
%rep 16                    ; IRQ0-15（PIC 重映射后位于 0x20-0x2F）
    STUB_NOERR i
%assign i i+1
%endrep

STUB_NOERR 0x80            ; 系统调用门（DPL=3，见 idt.c）

isr_common:
    push esp               ; 参数：struct regs *（esp 正指向 int_no）
    call isr_dispatch
    add esp, 4
    add esp, 4             ; 弹掉 int_no
    POPALL
    iretd

; ---- 入口表：0-31 异常，32-47 IRQ，48=int 0x80 ----
section .rodata
global isr_table
isr_table:
%assign i 0
%rep 32
    dd stub_ %+ i
%assign i i+1
%endrep
%assign i 32
%rep 16
    dd stub_ %+ i
%assign i i+1
%endrep
    dd stub_0x80
