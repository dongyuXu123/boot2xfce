# ROADMAP —— 渐进增量路线（每步可实测）

原则：
1. **每阶段都是可引导、可断言的完整快照**（复制上一阶段改，绝不推倒重来）；
2. 每阶段收尾 = `make test` 全绿 → git 提交（阶段即里程碑）；
3. 遇到原理性理解缺口，先读 `upstream/` 对应源码再动手（docs/ 有导读）。

## S0 加电与实模式 ✅

- [x] 复位向量 / BIOS POST / 0x7C00 / 0xAA55 引导签名
- [x] 实模式寻址、栈建立、BIOS int 10h 输出
- [x] 直接端口 IO：COM1 (0x3F8) 初始化与输出（测试通道）
- 验收：`stages/s0-mbr` make test —— 串口出现 `S0: power-on OK`

## S1 GRUB 引导 ✅

- [x] Multiboot 1 头（magic/flags/checksum），内核链接到 1MiB
- [x] grub-mkrescue 制 ISO（BIOS+EFI 混合），grub.cfg 自动引导
- [x] 解析 multiboot 信息：内存大小、E820 内存映射
- [x] **坑位记录**：GRUB2 的 multiboot1 加载器只认 flags `0x1|0x2|0x10000`，
      多一位（如 bit6 请求 mmap）直接 `unsupported flag` 拒载
- 验收：`stages/s1-grub` make test —— 串口出现 `S1: GRUB boot OK` + `stage1 done`

## S2 内核基础框架 ✅（进程 / 内存 / 文件 / API）

- [x] GDT（内核/用户段 + TSS）、IDT、8259A 重映射、PIT 100Hz
- [x] 异常兜底转储（int_no/eip/esp/cs/err/cr2）
- [x] 内存：multiboot E820 → 物理页位图；固定区堆 kmalloc/kfree（首次适应+合并）
- [x] 进程：每任务 4KiB 内核栈；xv6 式软件上下文切换；时间片轮转；
      内核任务蹦床 + ring3 用户蹦床（iretd 降级）
- [x] 文件：ramfs（创建/写/读/列目录，512B/文件）
- [x] API：int 0x80 = write/getpid/exit/uptime/ls；用户程序经 incbin 嵌入内核
- [x] **坑位记录**：
  - `struct context` 成员顺序必须与 switch_to 压栈顺序严格一致（ebp 在最低）；
  - 新任务初始 context 必须布在**它自己的内核栈顶**（把任务结构体当栈 = 踩穿 bss）；
  - 用户程序要 `-static -no-pie` 链接（Ubuntu gcc 默认 PIE，objcopy 出 4MB 平铺文件）；
  - 用户态内联汇编 `int 0x80` 要加 `"memory"` clobber（否则栈上数据未落内存内核读到旧值）。
- 验收：`stages/s2-kernel-core` make test —— 8 个标记全过（含 `[user] hello from ring3`）

## S3 丰富系统（下一站）

按依赖顺序推进，每项都先写进 `docs/20-kernel.md` 对照 `upstream/linux` 的实现：

1. **分页与虚存**：四级... 不，32 位两级页表；内核高位映射；每任务地址空间
   （对照 `upstream/linux/arch/x86/mm/`、`mm/`）
2. **进程进化**：fork（先普通拷贝，再 COW）、wait/exit 收尸（回收内核栈）、
   ELF 用户程序加载器（ramfs 里放 /bin/init）
3. **文件进化**：ramfs → 多态 vnode；Ext2 只读（盘镜像工具脚本）；
   fd 表 + open/read/write/close/lseek 全套 ABI
4. **输入输出**：键盘行规程（TTY 雏形）、块设备层（先 ATA PIO 再 virtio-blk）
5. **API 扩容**：系统调用表化；时间/日期；简单 shell（/bin/sh 内置命令）
- 验收：用户态 shell 从 Ext2 加载 ELF 并执行；两个进程 fork/通信演示

## S4 桌面结合（Xfce）

双轨制，A 先 B 后，最终对照合流：

**Track A —— 真栈复现（可脚本化、可复现）**
- host 上交叉编译裁剪 Linux（`upstream/linux`，tinyconfig 起步）+ BusyBox rootfs
- 逐个编译 `upstream/xfce/*` 核心模块（依赖序：libxfce4util → xfconf → libxfce4ui
  → garcon/exo → panel/xfwm4/xfdesktop/session/settings/appfinder）
- QEMU 图形启动完整 Xfce 会话；一切进 Makefile，一键复现

**Track B —— 自研内核 GUI（对照学习）**
- S3 内核加 framebuffer 驱动（QEMU BOCHS/STDVGA LFB）、字体渲染、窗口系统
- 参考 linux011-course day37-40 的窗口 API 经验做现代化重写
- 最终"结合"：Track A 的 Xfce 截图/交互作为对照标准，Track B 尽量对齐其桌面范式

- 验收：Track A 出完整桌面录像；Track B 出自研窗口演示 + 对照报告
