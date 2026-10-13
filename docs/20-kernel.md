# 20 · Linux 源码导读 ↔ 自研内核四件套（S2/S3）

`upstream/linux`（7.3-rc7 master）。读真实内核不为了照抄，为了**对照**：
自研框架每做一个决定，都去 Linux 里找同构物。

## 顶层目录速查（自研内核视角）

| Linux | 内容 | 我们 S2 的对应 |
|---|---|---|
| `arch/x86/` | 架构层：入口/页表/中断 | boot.asm, cpu.asm, isr.asm, gdt.c, idt.c |
| `arch/x86/kernel/apic/apic.c` → 旧式 `8259A` 在 `i8259.c` | PIC/中断控制器 | irq.c `pic_init()` |
| `arch/x86/kernel/time.c` + `drivers/clocksource/` | 时钟 | irq.c `pit_init()` |
| `mm/page_alloc.c` | 伙伴系统（物理页） | mm.c 位图 PMM（简化版） |
| `mm/slab.h` `mm/kmalloc` 相关 (slub) | 内核堆 | mm.c 首次适应堆 |
| `kernel/sched/core.c` `fair.c` | 调度类/CFS | task.c 轮转（RR） |
| `arch/x86/process.c` `entry_32.S` | 上下文切换/系统调用入口 | cpu.asm `switch_to`, isr.asm |
| `fs/` | VFS + 具体文件系统 | ramfs.c（还没 VFS 抽象） |
| `include/uapi/asm/unistd.h` | 系统调用号表 | kernel.h + syscall.c（硬编码 1-5） |

读源码建议以**数据结构**为锚：`struct page`（mm_types.h）、`struct task_struct`、
`struct file`/`struct inode`、`syscall table`。抓住"谁拥有谁的生命周期"。

## S2 已立起来的骨架（stages/s2-kernel-core）

- **内存**：multiboot E820 → 4KiB 位图 PMM（`pmm_alloc_frame/free_frame`）；
  固定区 `[2M,4M)` 堆：块头 12B + 首次适应 + 后向合并。
- **进程**：每任务 4KiB 内核栈（kmalloc）；中断现场天然留在被打断任务的栈上；
  `switch_to` 只换 esp + 4 个 callee-saved 寄存器（xv6/现代 Linux 同款）；
  蹦床制：内核任务 → `ktask_trampoline`，用户任务 → `utask_trampoline`（iretd 降 ring3）。
- **文件**：ramfs 固定表，8 文件 × 512B（S3 换 vnode + Ext2）。
- **API**：int 0x80，eax=号 ebx/ecx/edx=参。用户程序单独编译成平铺二进制，
  `incbin` 嵌入内核，启动拷到 `USER_BASE=0x400000`。

### ⚠️ S2 踩坑全记录（每条都是实测炸出来的）

1. **`struct context` 顺序 = 压栈顺序**：push ebx,esi,edi,ebp 后 ebp 在最低地址，
   struct 必须是 `{ebp,edi,esi,ebx,eip}`——反了就是 `ret` 进 0 地址，int 6。
2. **新任务初始 context 必须布在它自己的内核栈顶**。曾把 `&tasks[i].kctx`（.bss 里的
   结构体）当栈指针交给 switch_to——蹦床的压栈把整个 tasks[] 踩穿，症状诡异
   （任务状态莫名变 0、eip 全是 0）。定位手段：QEMU gdbstub + `watch` 观察点，
   抓到写入者 eip 在蹦床函数体内、写入值是 print 的返回地址——真相大白。
3. **用户程序要 `-static -no-pie`**：Ubuntu gcc 默认 PIE，`objcopy -O binary` 会从
   基址 0 一路铺到 4MB。另用汇编入口桩保证 `_start` 恰在 `USER_BASE`。
4. **用户态 `int 0x80` 内联汇编加 `"memory"` clobber**：否则 GCC 把栈缓冲的
   存储指令重排到 int 指令之后，内核从用户栈拷到内核时读到旧数据（打印空数字）。
5. **GRUB flags 坑**：见 `docs/10-grub.md`（0x43 被拒载）。
6. **PMM 必须保留低 1MiB**：位图帧号×4K=物理地址，alloc 从帧 0 扫起会把
   0x0 当可分配页——教训：只打总数统计的测试不算测试，alloc/free 行为
   断言（连续分配不同、释放复用同页）才算。
7. **IF 不变量**：switch_to 不保存 EFLAGS，第一次切入内核任务时 IF 还是
   中断门清掉的 0——内核任务蹦床开头必须 `sti()`，此后 IF 由各任务自己的
   iret 帧保存/恢复。症状：PIC 的 IRR 里键盘中断永远 pending，系统只剩
   软触发中断在空转。由 L18 的 QEMU sendkey 实测揪出。

### 当前已知限制（= S3 的任务清单）

- 无分页：ring3 与 ring0 共地址空间，**用户态没有隔离**（段都是平的）；
- 无 fork/exec：任务只能创建时静态指定入口；
- DEAD 任务内核栈与 PCB 永不回收（无 reap）；
- ramfs 无目录、容量固定；系统调用指针参数直接用（无合法用户指针校验）。

## S3 推进顺序（每步都有 upstream 对照）

1. 两级页表分页：内核恒高映射 + `mm/page_alloc.c` 读 `free_area_init`
2. COW fork：页错误驱动复制 —— 对照 `mm/memory.c` `do_wp_page`
3. ELF 加载器 —— 对照 `fs/binfmt_elf.c`（只读 PT_LOAD 段映射那部分）
4. fd 表 + VFS 雏形（file_op 结构体）—— 对照 `fs/read_write.c`
5. Ext2 只读 —— 对照 `fs/ext2/`，配合仓库根 `tools/mkext2.py` 制盘
6. 系统调用表 + 用户态微型 libc —— 对照 `arch/x86/entry/vsyscall`
