# L17 · ring3 用户程序

**目标**：让代码第一次跑在特权级 3——你的内核从此有"用户"了。
**你亲手写**（两个文件）：
1. `user.c` 的 `sys()`：内联汇编 `int $0x80`，输出约束 `"=a"(r)`，
   输入 `"a"(n),"b"(a),"c"(b),"d"(c)`，**必须带 `"memory" clobber`**——
   否则 GCC 会把栈缓冲的存储指令重排到 int 之后，内核从用户栈拷到的
   还是旧内存（症状：打印的数字神秘消失）。
2. `cpu.asm` 的 `utask_trampoline`：依次 push 用户 ss(0x23) / esp / 
   eflags(0x202，IF=1) / cs(0x1B，RPL=3) / eip，然后 `iretd` 一次性降级。
   想想：进 ring3 前谁把 TSS.esp0 指到本任务的内核栈顶？（schedule 干的）

**背景**：用户程序单独编译（`-static -no-pie`！Ubuntu 默认 PIE 会让
平铺二进制膨胀 4MB）成平铺二进制，`incbin` 嵌进内核，启动时拷到
USER_BASE=0x400000。无分页所以 ring3 没有真隔离——S3 的活。

**验收**：串口出现 `[user] hello from ring3, pid=4`（还有 uptime 和 ls）。
**参考答案**：`stages/s2-kernel-core/{cpu.asm,user.c}`。
