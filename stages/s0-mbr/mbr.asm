; ============================================================
; S0 —— 加电第一步（实模式 16 位）
;
; x86 加电/复位后 CPU 从 0xFFFF0 开始执行，BIOS 完成自检后按启动顺序
; 把第一个可引导设备的第一个扇区（512 字节，末尾必须是 0xAA55）加载到
; 物理地址 0x7C00，然后跳过去执行 —— 这就是 MBR。
;
; 本阶段：证明"我们接管了机器"。输出走两路：
;   - BIOS int 10h → VGA 文本（人看的）
;   - 直接写 COM1 端口 0x3F8 → 串口（QEMU 无头测试断言用）
; ============================================================
[BITS 16]
[ORG 0x7C00]

COM1 equ 0x3F8

start:
    cli
    xor ax, ax              ; 初始化段寄存器：DS/ES/SS=0
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00          ; 栈向下生长，放 MBR 自己下面
    sti

    call serial_init

    mov si, msg
.print:
    lodsb
    or al, al
    jz .hang
    mov ah, 0x0E            ; BIOS TTY 输出（电传模式，到 VGA）
    mov bx, 0x0007          ; 页0 / 黑底白字
    int 0x10
    call serial_putc        ; 同一个字符也送 COM1
    jmp .print
.hang:
    hlt
    jmp .hang

; ---- 初始化 COM1：115200 8N1，FIFO 开 ----
serial_init:
    mov dx, COM1+1
    xor al, al              ; 禁用串口中断
    out dx, al
    mov dx, COM1+3
    mov al, 0x80            ; DLAB=1，允许设置波特率
    out dx, al
    mov dx, COM1
    mov al, 0x01            ; 除数低字节=1（115200）
    out dx, al
    mov dx, COM1+1
    xor al, al              ; 除数高字节=0
    out dx, al
    mov dx, COM1+3
    mov al, 0x03            ; 8N1，DLAB=0
    out dx, al
    mov dx, COM1+2
    mov al, 0xC7            ; 开 FIFO、清队列
    out dx, al
    mov dx, COM1+4
    mov al, 0x0B            ; DTR+RTS+OUT2
    out dx, al
    ret

; 入口 AL=字符；保留 AX
serial_putc:
    push ax
    mov dx, COM1+5
.wait:
    in al, dx
    test al, 0x20           ; 发送保持寄存器空？
    jz .wait
    pop ax
    mov dx, COM1
    out dx, al
    ret

msg: db "S0: power-on OK - BIOS loaded MBR at 0x7C00, real mode alive", 0x0D, 0x0A, 0

; 补零到 510 字节，最后两字节是引导签名
times 510-($-$$) db 0
dw 0xAA55
