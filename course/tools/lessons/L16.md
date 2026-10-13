# L16 · int 0x80 系统调用

**目标**：定义你这台机器的 ABI——用户程序与内核的唯一合法通道。
**你亲手写**（`syscall.c` 五个 case）：
约定 `eax`=调用号、`ebx/ecx/edx`=参数、返回值写回 `eax`：
- `1 write(fd,buf,len)`：n=min(edx,255) → memcpy 进内核 kbuf → 补 \0 →
  print → 返回 n（无分页=同地址空间，指针直用；S3 开分页后这里必须换
  "只翻一层"拷贝语义，这是用户指针安全的第一课）
- `2 getpid`：current->pid
- `3 exit`：current->state=T_DEAD（想想为什么不能在这里 kfree 自己的栈）+
  打印 `S2: [user] exit, pid=N`
- `4 uptime`：返回 ticks
- `5 ls`：ramfs_list()，返回 0

**验收**：kmain 在 ring0 里直接 `int $0x80` 调 write 打
`S2: syscall OK`（桩全返回 -1，必红）。
**参考答案**：`stages/s2-kernel-core/syscall.c`。
