; 用户程序二进制经 incbin 直接嵌进内核映像；kmain 侧把它拷到 USER_BASE。
section .userblob
global user_blob, user_blob_end
user_blob:
    incbin "build/user.bin"
user_blob_end:
