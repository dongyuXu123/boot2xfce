# L14 · 任务与调度（全课程最重的一课）

**目标**：让两个任务在同一颗 CPU 上"同时"跑——上下文切换的每根神经
都要你亲手接。
**你亲手写**（`cpu.asm` 的 `switch_to` + `task.c` 五件套）：
1. `switch_to(old_slot, new_esp)`：参数进 eax/edx → push ebx,esi,edi,ebp
   （压完 ebp 在最低！）→ `[old]=esp` → `esp=new` → pop 四连 → `ret`
   （ret 消费的正是新上下文的 eip）。
2. `ktask_trampoline`：`current->entry()` → 置 DEAD → `for(;;) hlt()`。
3. `task_create`：找 T_EMPTY 槽 → 填 pid/T_READY/entry/is_user →
   kmalloc 4KiB 内核栈 → **在栈顶布置初始 context**（`ctx = kstack_top -
   sizeof(struct context)`，清零，`eip=蹦床`，`kesp=(uint32_t)ctx`）→
   is_user 时填 utask_eip/utask_esp。
   ⚠️ kesper绝不能指向 tasks[] 里的任何地址——把 .bss 结构体当栈用，
   第一次切换就踩穿任务表（参考答案 docs/20-kernel.md 坑位 #2，
   gdb watchpoint 实录）。
4. `schedule`：static last 环形扫 T_READY → prev RUNNING→READY →
   current=next → is_user 设 TSS esp0 → `switch_to(&prev->kesp, next->kesp)`。
5. `sched_loop`：`for(;;){ sti(); hlt(); schedule(); }`；`task_yield`：
   内联汇编 `int $0x20`（软触发 IRQ0 的门）。

**验收**：taskA/taskB 交替打印 `taskA run 1` / `taskB run 1`。
**参考答案**：`stages/s2-kernel-core/{cpu.asm,task.c}`。
