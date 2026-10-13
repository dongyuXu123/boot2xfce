; L10 桩 —— 中断/异常入口桩
; ================================================================
; 任务（见任务书）：写 49 个入口 + 公共出口。
; 栈布局必须与 kernel.h 的 struct regs 严格一致（低→高）：
;   int_no | gs fs es ds | edi..eax | (异常错误码提前弹走) | eip cs eflags [esp ss]
; 带错误码的异常（8,10-14,17,21,29,30）先用 pop 把 CPU 压的错误码
; 弹进全局变量 isr_err_code（C 侧），再统一栈型。
; 公共出口：push esp(=regs*) → call isr_dispatch → 平栈 → POPALL → iretd
;
; 下面给一个能编译但啥也不干的占位实现——所以只写 idt.c 是不够的，
; 中断来了会走这个哑桩，PIT/PIC/系统调用全都哑火。
; ================================================================
[bits 32]

section .text
global isr_table

stub_dummy:
    iretd

section .rodata
isr_table:
%assign i 0
%rep 49
    dd stub_dummy
%assign i i+1
%endrep
