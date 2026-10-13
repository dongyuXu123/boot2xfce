; ================================================================
; L01 解答 —— 加电第一步：引导扇区 + BIOS 打印
; 机器加电后：BIOS 自检 → 读启动盘第一个扇区(512B)到 0x7C00
; → 检查末尾签名 0xAA55 → 跳过来执行。从这里开始，机器归我们管。
; ================================================================
[BITS 16]                       ; 实模式：16 位代码
[ORG 0x7C00]                    ; 告诉汇编器：这段代码会被装到 0x7C00，
                                ; 所有标号按这个基地址算偏移

COM1 equ 0x3F8                  ; COM1 串口的首端口地址

start:
    cli                         ; 先关中断：段寄存器没设好前来中断会乱套
    xor ax, ax                  ; ax = 0
    mov ds, ax                  ; 数据段 = 0（lodsb 取数用 ds:si）
    mov es, ax                  ; 附加段 = 0（约定俗成清干净）
    mov ss, ax                  ; 栈段 = 0
    mov sp, 0x7C00              ; 栈顶放在代码开头：push 向下长，
                                ; 正好往 0x7C00 以下的空地长，不碰代码
    sti                         ; 段都就位了，开中断

    call serial_init            ; 给定设施：串口初始化（L02 才轮到我们写）

; ---------- 作业①：打印循环 ----------
    mov si, msg                 ; si 指向字符串首地址（ds:si）
.print:
    lodsb                       ; 取 ds:si 的一个字节到 AL，si 自动 +1
    or al, al                   ; 判断 AL 是否为 0（or 自己=只看标志不改值）
    jz .hang                    ; 0 = 字符串结束，跳去停机
    mov ah, 0x0E                ; BIOS 电传输出功能号
    mov bx, 0x0007              ; BH=显示页 0，BL=颜色（黑底白字）
    int 0x10                    ; 调 BIOS，把 AL 里的字符画到屏幕
    call serial_putc            ; 同一个字符也送串口（测试断言走这条路）
    jmp .print                  ; 处理下一个字符

.hang:
    hlt                         ; 停 CPU 等中断（没事干也不空转）
    jmp .hang                   ; 万一被唤醒，继续睡

; ---------- 给定设施：COM1 初始化 115200 8N1 ----------
serial_init:
    mov dx, COM1+1
    xor al, al                  ; IER：关串口自身中断
    out dx, al
    mov dx, COM1+3
    mov al, 0x80                ; LCR：DLAB=1，下面两步要设波特率
    out dx, al
    mov dx, COM1
    mov al, 0x01                ; 除数低字节 = 1（1.8432MHz/16/1 = 115200）
    out dx, al
    mov dx, COM1+1
    xor al, al                  ; 除数高字节 = 0
    out dx, al
    mov dx, COM1+3
    mov al, 0x03                ; LCR：8 数据位 无校验 1 停止位，DLAB=0
    out dx, al
    mov dx, COM1+2
    mov al, 0xC7                ; FIFO：开并清空
    out dx, al
    mov dx, COM1+4
    mov al, 0x0B                ; MCR：DTR|RTS|OUT2
    out dx, al
    ret

; ---------- 给定设施：发送 AL 到 COM1（保护 AX，调用方还在用） ----------
serial_putc:
    push ax
    mov dx, COM1+5
.wait:
    in al, dx                   ; 读线路状态寄存器
    test al, 0x20               ; bit5=1 表示发送保持寄存器空
    jz .wait                    ; 没空就原地等
    pop ax
    mov dx, COM1
    out dx, al                  ; 字符写进数据口，发出去
    ret

; ---------- 作业②：要打印的字符串 ----------
; 注意 \r\n（0x0D 0x0A）：串口终端要回车+换行才肯回到行首
msg: db "S0: power-on OK - BIOS loaded MBR at 0x7C00, real mode alive", 0x0D, 0x0A, 0

; ---------- 作业③：补零到 510 字节 + 引导签名 ----------
times 510-($-$$) db 0           ; $=当前位置 $$=文件起点；差多少补多少零
dw 0xAA55                       ; 魔数签名（小端，落盘就是 55 AA）——
                                ; BIOS 靠它认定"这是个引导扇区"
