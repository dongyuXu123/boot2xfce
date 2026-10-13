; ============================================================
; cpu.asm —— CPU 级原语：GDT 重载 / TSS 装载 / 上下文切换 / ring3 进入
; ============================================================
[bits 32]

section .data
global utask_eip, utask_esp
utask_eip: dd 0                 ; ring3 代码入口
utask_esp: dd 0                 ; ring3 栈顶

section .text
global gdt_flush, tss_flush, switch_to, utask_trampoline, utask_trampoline_forked
extern gp                       ; gdt.c 里的 GDTR

; void gdt_flush(uint32_t gdtr_ptr)
gdt_flush:
    mov eax, [esp+4]
    lgdt [eax]
    mov ax, 0x10                ; 重载数据段
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:.flush             ; 远跳重载 CS
.flush:
    ret

; void tss_flush(void)
tss_flush:
    mov ax, 0x2B                ; TSS 描述符选择子（GDT 第 5 项）
    ltr ax
    ret

; void switch_to(struct context *old, struct context *new)
; 经典软件上下文切换（xv6 风格）：只存被调方保存寄存器 + 栈指针，
; 中断现场自然留在各自任务内核栈上，恢复后各自完成 iret。
; 压栈顺序 ebx,esi,edi,ebp → 栈上 ebp 最低；struct context 顺序与此一致。
switch_to:
    mov eax, [esp+4]            ; old
    mov edx, [esp+8]            ; new
    push ebx
    push esi
    push edi
    push ebp
    mov [eax], esp              ; 保存当前上下文
    mov esp, edx                ; 换栈
    pop ebp
    pop edi
    pop esi
    pop ebx
    ret                         ; ret 即跳到 new->eip

; ring3 任务首次被调度时经 kctx.eip 到达这里。
; 创建任务时已在它自己的内核栈上预制好 iret 帧（低→高：
; eip,cs,eflags,esp,ss），帧地址放在 ctx.ebx——每个任务一份，
; 不再依赖全局变量（fork 的多个用户任务各自有不同的进入点）。
utask_trampoline:
    mov esp, ebx                ; esp ← 本任务预制 iret 帧
    iretd                       ; 一发降级 ring3

; fork 出的任务走这里：ebx 指向预制好的"段+通用寄存器影像+iret 帧"，
; 先恢复段与通用寄存器（eax=0 → 子进程 fork() 返回 0），再 iretd。
; 影像布局（低→高）：gs fs es ds | edi esi ebp (esp占位) ebx edx ecx eax | iret帧
utask_trampoline_forked:
    mov esp, ebx
    pop gs
    pop fs
    pop es
    pop ds
    popa                        ; 内部自带 esp 占位槽的跳过，外部不要再加
    iretd
