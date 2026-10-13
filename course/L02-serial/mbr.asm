; ================================================================
; L02 桩 —— 亲手写 COM1 串口驱动
; 上一课的 BIOS 打印已经就位（保留你的成果），本课把串口两件套
; 从"给定设施"降级为 TODO：初始化 + 发送。
; ================================================================
[BITS 16]
[ORG 0x7C00]

COM1 equ 0x3F8

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    call serial_init

    mov si, msg
.print:
    lodsb
    or al, al
    jz .hang
    mov ah, 0x0E
    mov bx, 0x0007
    int 0x10
    call serial_putc
    jmp .print
.hang:
    hlt
    jmp .hang

; TODO(你来写·L02): 初始化 COM1 为 115200 8N1、开 FIFO
; 步骤（对照任务书和 stages/s0-mbr/mbr.asm 的答案）：
;   1) IER(COM1+1) 写 0          关串口中断
;   2) LCR(COM1+3) 写 0x80       DLAB=1
;   3) DLL(COM1) 写 1, DLM 写 0  除数=1 → 115200
;   4) LCR 写 0x03               8N1，DLAB=0
;   5) FCR(COM1+2) 写 0xC7       FIFO
;   6) MCR(COM1+4) 写 0x0B       DTR|RTS|OUT2
serial_init:
    ret                         ; TODO: 替换我

; TODO(你来写·L02): 发送 AL 到 COM1（必须保留 AX！主循环还要用它）
; 步骤：先轮询 LSR(COM1+5) 的 bit5（发送保持寄存器空），
;       再把 AL 写进数据口 COM1。
serial_putc:
    ret                         ; TODO: 替换我

msg: db "S0: power-on OK - BIOS loaded MBR at 0x7C00, real mode alive", 0x0D, 0x0A, 0

times 510-($-$$) db 0
dw 0xAA55
