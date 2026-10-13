; ================================================================
; L03 桩 —— 十六进制打印（寄存器/循环/移位练习）
; 主消息与串口驱动已是你的成果；本课写 print_ax_hex：
; 把 AX 按四位大写十六进制打出（BIOS+串口双路，putc2 已给定）。
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
    jz .hex
    mov ah, 0x0E
    mov bx, 0x0007
    int 0x10
    call serial_putc
    jmp .print

; 十六进制自检：AX 固定 0x1234，打出 "AX=1234"
.hex:
    mov si, msg2
.p2:
    lodsb
    or al, al
    jz .hang
    mov ah, 0x0E
    mov bx, 0x0007
    int 0x10
    call serial_putc
    jmp .p2

; 十六进制自检：打印 "AX=" 后由你补的 print_ax_hex 打出 "1234"
.hang:
    mov ax, 0x1234              ; TODO(你来写·L03): 调 print_ax_hex 前装载 0x1234
    call print_ax_hex           ; ← 打完应出现 "AX=1234"
.hang2:
    hlt
    jmp .hang2

; TODO(你来写·L03): 打印 AX 的 4 位大写十六进制（每位经 putc2 双路输出）
; 思路：循环 4 次 { 循环左移 4 位 → 取低 4 位 → 0-9 加 '0'、A-F 加 'A'-10 → putc2 }
; 注意：putc2 会改 AX/别的寄存器吗？看它的实现，该保护就保护。
print_ax_hex:
    ret                         ; TODO: 替换我

; ---- 给定：AL 双路输出（BIOS int 10h + COM1）----
putc2:
    push ax
    mov ah, 0x0E
    mov bx, 0x0007
    int 0x10
    pop ax
    push ax
    call serial_putc
    pop ax
    ret

; ---- 你的成果（L02）：COM1 驱动 ----
serial_init:
    mov dx, COM1+1
    xor al, al
    out dx, al
    mov dx, COM1+3
    mov al, 0x80
    out dx, al
    mov dx, COM1
    mov al, 0x01
    out dx, al
    mov dx, COM1+1
    xor al, al
    out dx, al
    mov dx, COM1+3
    mov al, 0x03
    out dx, al
    mov dx, COM1+2
    mov al, 0xC7
    out dx, al
    mov dx, COM1+4
    mov al, 0x0B
    out dx, al
    ret

serial_putc:
    push ax
    mov dx, COM1+5
.wait:
    in al, dx
    test al, 0x20
    jz .wait
    pop ax
    mov dx, COM1
    out dx, al
    ret

msg:  db "S0: power-on OK - BIOS loaded MBR at 0x7C00, real mode alive", 0x0D, 0x0A, 0
msg2: db "S0: hex self-test AX=", 0

times 510-($-$$) db 0
dw 0xAA55
