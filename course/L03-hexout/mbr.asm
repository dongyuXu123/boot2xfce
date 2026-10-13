; ================================================================
; [课程专用答案] L03 的 print_ax_hex 参考解
; （L03 的十六进制打印是课程新增练习，stages/ 里没有对应文件，
;   所以这份答案单独放在课程里，verify.sh 用它做"答案必绿"验收）
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

.hex:
    mov si, msg2
.p2:
    lodsb
    or al, al
    jz .hexgo
    mov ah, 0x0E
    mov bx, 0x0007
    int 0x10
    call serial_putc
    jmp .p2

.hexgo:
    mov ax, 0x1234
    call print_ax_hex
.hang:
    hlt
    jmp .hang

; 参考解：AX 的 4 位大写十六进制，经 putc2 双路输出
print_ax_hex:
    push cx
    push ax
    mov cx, 4
.l:
    rol ax, 4                   ; 最高 4 位转到最低
    push ax
    and al, 0x0F
    cmp al, 10
    jb .digit
    add al, 'A'-10-'0'          ; A-F：先补差再统一加 '0'
.digit:
    add al, '0'
    call putc2
    pop ax
    loop .l
    pop ax
    pop cx
    ret

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
