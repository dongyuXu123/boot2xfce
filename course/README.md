# boot2xfce 动手课程 —— 每课都亲手写，test 说了算

> `stages/` 是**参考答案**（大框架，不动它）；`course/` 是**练习场**。
> 每课 = 一个可构建的完整小项目，其中本课的新功能被挖成 `TODO(你来写)` 桩——
> 编译能过、功能没有、`make test` 必红；你按任务书写完，test 转绿才算过关。

## 学习方法（每课固定四步）

```bash
cd course/L09-gdt          # 1. 进课
cat README.md              # 2. 读任务书：目标/你写什么/提示/验收
vim gdt.c                  # 3. 只写标了 TODO(你来写) 的地方，别的地方别动
make test                  # 4. 串口断言验收，绿了 git commit，进下一课
```

- 卡住了？`tools/stubs/LNN/` 旁边就是 `stages/` 参考答案，先自己撞 30 分钟再看。
- 写崩了想重来？`tools/gen-lesson.sh L09` 一键重置本课。
- 每课的 test 会**重考之前所有课**（回归是内置的），改坏了旧功能也会红。

## 分课表

### 第一阶段 · S0 从加电开始（MBR 实模式）
| 课 | 小功能 | 你亲手写 |
|---|---|---|
| L01 | 引导扇区与 BIOS 输出 | 512B 引导扇区：栈/`int 10h` 打印/0xAA55 |
| L02 | COM1 串口驱动 | UART 初始化 + 按位轮询发送（测试通道） |
| L03 | 十六进制打印 | 汇编循环/移位，为内核调试铺路 |

### 第二阶段 · S1 GRUB 引导（Multiboot）
| 课 | 小功能 | 你亲手写 |
|---|---|---|
| L04 | Multiboot 头与 1MiB 布局 | multiboot 头三件套 + 链接脚本 |
| L05 | grub.cfg 与 ISO 制作 | grub.cfg + Makefile 的 grub-mkrescue 规则 |
| L06 | 内存大小解析 | multiboot 信息结构 mem_lower/mem_upper |
| L07 | E820 内存映射 | mmap 链表遍历（变长记录 + 步进陷阱） |

### 第三阶段 · S2 内核基础框架（每个子系统逐课立起来）
| 课 | 小功能 | 你亲手写 |
|---|---|---|
| L08 | 双路 console | 串口+VGA print/print_hex/print_dec |
| L09 | GDT 与 TSS 描述符 | 6 个描述符 + lgdt 重载（sgdt 自检） |
| L10 | IDT 与异常桩表 | 256 门 + sidt 自检 + 入口桩 |
| L11 | PIC 重映射 + PIT 时钟 | 8259A ICW 流程 + 100Hz 定时（ticks 实测） |
| L12 | 物理页位图 PMM | E820→位图，alloc/free（含保留区） |
| L13 | 内核堆 kmalloc | 首次适应 + 分裂 + 合并（复用实测） |
| L14 | 任务与调度 | switch_to 上下文切换 + 蹦床 + 轮转调度 |
| L15 | 内存盘 ramfs | write/read/list（读写回环实测） |
| L16 | int 0x80 系统调用 | 分发器五调用（内核态实测） |
| L17 | ring3 用户程序 | 平铺二进制嵌入 + iretd 降级 |
| L18 | 键盘 IRQ1 | 通码→ASCII 环形缓冲（QEMU 注键实测） |
| L19 | 毕业设计 | 自己设计第 6 个系统调用 + 自加断言 |

### 第四阶段 · S3 丰富系统（随实现滚动开课，规划 12 课）
分页(3)：页表建立 / 内核高映射 / 按需映射与缺页 · COW fork(2) · ELF 装载(2) ·
VFS 与 fd 表(2) · Ext2 只读(2) · TTY 行规程 + shell(2)。每实现一个功能，
立刻在 course/ 切成一课（桩+任务书），保持本表节奏。

### 第五阶段 · S4 桌面结合（规划）
Track A 真栈：裁剪 linux + BusyBox + 逐模块编 Xfce（每模块一课）；
Track B 自研：framebuffer / 字体 / 窗口系统 / 演示应用（每层一课）。

## 讲义（每课：为什么 + 坑与历史错误）

- `course/notes/L01-讲义.md` … `L19-讲义.md`：逐课讲义，与任务书配套——
  任务书说"写什么"，讲义说"为什么"与"哪里有坑"。
- 全部"坑与历史错误"来自本项目 S3/S4 实战（精确到起源与修复），
  例如 L12 的低 1MiB 保留、L14 的 switch_to 不携带 EFLAGS、
  L16 的 exit 必须切走、L17 的 PIE 平铺陷阱等。

## 工具

```bash
tools/gen-lesson.sh            # （重新）生成全部课程
tools/gen-lesson.sh L09        # 只重置某一课
tools/verify.sh                # 教师视角：全课程"桩必红/答案必绿"体检
```

## 给课程维护者（未来的我们）

- `stages/` 是唯一事实源：修了 stages 的 bug，跑一遍 `tools/gen-lesson.sh` 即可
  把修复同步进所有课（生成是幂等的，课程目录不入心的只有你的解答）。
- 新课 = 在 `tools/stubs/LNN-xxx/` 放桩文件 + `tools/lessons/LNN-xxx.md` 放任务书，
  在 `gen-lesson.sh` 的课程表里加一行。
