# boot2xfce —— 从加电到桌面的渐进式 OS 之旅

从 x86 加电那一刻开始，一步一步亲手把一台"机器"带进图形桌面时代：

```
加电/BIOS ──► GRUB 引导 ──► 自研内核(进程/内存/文件/系统调用) ──► 丰富系统 ──► Xfce 桌面结合
   S0            S1                S2                          S3                S4
```

**模式：渐进增量 + 每步可复现可实测。** 每个阶段（`stages/s*`）都是完整可构建的快照，
`make test` 在 QEMU 里无头启动、串口断言、自动判定通过/失败——没有任何"应该能跑"。

## 快速开始

```bash
# 依赖（Ubuntu）：gcc(-m32) nasm grub-pc-bin grub-common xorriso mtools qemu-system-x86
make test-all        # 依次构建并 QEMU 实测 S0/S1/S2
make -C stages/s2-kernel-core run   # 想亲眼看：带图形窗口启动 S2
```

通过标志：串口日志中出现各阶段约定标记（见各阶段 Makefile 的 test 目标）。

## 目录

| 路径 | 内容 |
|---|---|
| `stages/s0-mbr/` | S0 实模式 MBR：BIOS 加电 → 0x7C00，VGA+串口问好 |
| `stages/s1-grub/` | S1 GRUB(Multiboot) 引导最小内核，解析内存信息 |
| `stages/s2-kernel-core/` | S2 内核基础框架：物理内存+堆 / 任务+轮转调度 / ramfs / int 0x80 + ring3 |
| `stages/s3-enrich/` | S3 规划占位（见 ROADMAP） |
| `stages/s4-desktop/` | S4 规划占位（见 ROADMAP） |
| `docs/` | 源码导读与原理笔记（加电 / GRUB / Linux / Xfce） |
| `upstream/` | 参考源码：GRUB、Linux、Xfce（不入库，见 MANIFEST.md） |
| `scripts/` | 上游取源脚本、工具链版本记录 |

## 可复现模型

1. **上游钉版本**：`upstream/` 里每个仓库的确切 commit 记录在 `MANIFEST.md`，
   `scripts/fetch-upstream.sh` 可在任何机器上复现同样的拉取（镜像优先、官方回退）。
2. **工具链留痕**：`scripts/check-env.sh` 把 gcc/nasm/qemu/grub 版本写进 `build/env.txt`。
3. **确定性测试**：QEMU 无头 + 超时熔断 + 串口断言，CI 友好。

## 当前状态

- ✅ S0：MBR 实模式引导（QEMU 实测通过）
- ✅ S1：GRUB Multiboot 引导 + multiboot 信息解析（QEMU 实测通过）
- ✅ S2：内核四件套最小闭环（QEMU 实测通过）：
  - 内存：E820 位图物理页管理 + 首次适应堆
  - 进程：xv6 式内核栈上下文切换 + 100Hz 时间片轮转 + ring3 用户任务
  - 文件：内存盘 ramfs（写/读/列目录）
  - API：int 0x80（write/getpid/exit/uptime/ls）
- ⏳ S3/S4：规划见 [ROADMAP.md](ROADMAP.md)
