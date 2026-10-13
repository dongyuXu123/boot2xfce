; 用户程序入口桩：保证 _start 位于镜像最前（内核 iret 后跳 USER_BASE）
[bits 32]
section .text.start
global _start
extern user_main

_start:
    call user_main
.hang:
    jmp .hang
