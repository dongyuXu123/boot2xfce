; ================================================================
; L01 桩 —— 加电第一步：引导扇区 + BIOS 打印
; 标了 TODO(你来写) 的地方才是你的作业，其余是给定的设施/前课成果。
; 任务书：见本目录 README.md
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

    call serial_init            ; 给定：串口测试通道（L02 你会亲手写它）

    mov si, msg                 ; TODO(你来写·L01): 打印循环
.print:                         ;  逐字符取 si 指向的字节：
    lodsb                       ;  lodsb 取一个字符到 AL 并 si++
    or al, al                   ;  0 结尾则跳 .hang
    jz .hang                    ;  否则 BIOS int 10h(ah=0Eh,bx=0007) 打印
    mov ah, 0x0E                ;  再 call serial_putc 送串口，循环
    mov bx, 0x0007              ;  （上面 5 行是提示，注释只是路标，
    int 0x10                    ;   真正的取字符/判零/循环控制由你组织）
    call serial_putc
    jmp .print
.hang:
    hlt
    jmp .hang

; ---- 给定：COM1 初始化 115200 8N1（L02 课你将亲手重写它）----
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

; ---- 给定：发送 AL 到 COM1（保留 AX）----
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

; TODO(你来写·L01): 把下面占位串替换成任务书要求的完整字符串
; 内容必须是（0 结尾，含 \r\n）:
;   "S0: power-on OK - BIOS loaded MBR at 0x7C00, real mode alive"
; 提示: db "...", 0x0D, 0x0A, 0
msg: db 0

; TODO(你来写·L01): 补零到 510 字节 + 引导签名 0xAA55
; 提示: times 510-($-$$) db 0   和   dw 0xAA55
; 没有 0xAA55，BIOS 根本不认为这是引导扇区！
