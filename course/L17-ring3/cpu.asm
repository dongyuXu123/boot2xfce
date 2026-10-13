; L17 桩 —— ring3 进入蹦床（switch_to 是你 L14 的成果，保留）
; 本课只写 utask_trampoline：构造 iret 帧一次性降级到 ring3。
; 验收：用户程序真的跑起来（串口出现 [user] hello ...）。
; ================================================================
[bits 32]

section .data
global utask_eip, utask_esp
utask_eip: dd 0                 ; ring3 代码入口（task_create 填 USER_BASE）
utask_esp: dd 0                 ; ring3 栈顶（task_create 填 USER_TOP）

section .text
global gdt_flush, tss_flush, switch_to, utask_trampoline
extern gp

; 你的 L14 成果：gdt_flush
gdt_flush:
    mov eax, [esp+4]
    lgdt [eax]
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:.flush
.flush:
    ret

; 给定：tss_flush
tss_flush:
    mov ax, 0x2B
    ltr ax
    ret

; 你的 L14 成果：switch_to
switch_to:
    mov eax, [esp+4]            ; old
    mov edx, [esp+8]            ; new
    push ebx
    push esi
    push edi
    push ebp
    mov [eax], esp
    mov esp, edx
    pop ebp
    pop edi
    pop esi
    pop ebx
    ret

; TODO(你来写·L17): 构造 iret 帧降级 ring3
; 依次 push（栈从高到低的语义顺序）：
;   用户 ss = 0x23          ← push 最先（最外）
;   用户 esp = [utask_esp]
;   eflags = 0x202          ← IF=1，别的位为什么必须清零？
;   用户 cs = 0x1B          ← RPL=3，和 GDT 里 0x18 的关系？
;   用户 eip = [utask_eip]  ← push 最后（CPU 先取它）
; 然后 iretd。
; 想想：进 ring3 前谁负责把 TSS.esp0 指到本任务内核栈顶？（提示：schedule）
utask_trampoline:
    iretd                       ; TODO: 替换我
