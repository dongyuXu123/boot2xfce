; L14 桩 —— 上下文切换（本课的重头戏，也是全课程最经典的坑）
; gdt_flush/tss_flush/utask_trampoline 是给定成果；你写 switch_to。
; ================================================================
; switch_to(old_esp_slot, new_esp) 约定（与 kernel.h 的注释严格一致）：
;   1) eax/edx 取两个 CDECL 参数
;   2) 依次 push ebx, esi, edi, ebp  ← 压完后 ebp 在最低地址
;   3) 把当前 esp 存进 [old_esp_slot]
;   4) esp ← new_esp
;   5) pop ebp, edi, esi, ebx，最后 ret —— ret 消费的就是 new->eip
;
; struct context = { ebp, edi, esi, ebx, eip }（成员顺序=内存顺序=栈顺序！）
; 想清楚：为什么 eip 在最后？为什么不需要保存 eax/ecx/edx（CDECL 谁负责）？
; ================================================================
[bits 32]

section .data
global utask_eip, utask_esp
utask_eip: dd 0                 ; ring3 代码入口
utask_esp: dd 0                 ; ring3 栈顶

section .text
global gdt_flush, tss_flush, switch_to, utask_trampoline
extern gp                       ; gdt.c 里的 GDTR

; 给定：void gdt_flush(uint32_t gdtr_ptr)
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

; 给定：void tss_flush(void)
tss_flush:
    mov ax, 0x2B
    ltr ax
    ret

; TODO(你来写·L14): 按文件头约定实现
switch_to:
    ret                         ; TODO: 替换我

; TODO(你来写·L17): ring3 进入蹦床（本课先别动，iretd 占位即可编译）
; 到 L17 课时：依次 push 用户 ss(0x23)/esp/eflags(0x202)/cs(0x1B)/eip，
; 然后 iretd 一次性降级到 ring3。utask_eip/utask_esp 由 task_create 填好。
utask_trampoline:
    iretd                       ; TODO(L17): 替换我
