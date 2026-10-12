# 10 · GRUB 源码导读与 Multiboot 协议（S1）

`upstream/grub`（Savannah master，MANIFEST 钉 commit）。GRUB 是 GNU 的引导器，
我们要弄清两件事：**它怎么接管机器**，以及**它怎么把内核交给我们**。

## 源码地图

| 目录 | 内容 |
|---|---|
| `grub-core/` | 内核与全部模块：`kern/`（核心）、`disk/`、`fs/`、`loader/`（加载各类内核：`i386/multiboot_mbi.c` 就是 multiboot1 加载器！） |
| `grub-core/boot/i386/pc/` | `boot.S`（MBR 引导扇区）、`diskboot.S`（第二扇区） |
| `include/grub/` | 公共头：`multiboot.h`、`multiboot2.h` |
| `util/` | 宿主侧工具：`grub-mkrescue.c`、`grub-mkimage.c` |
| `grub-core/normal/` | `normal.mod`：菜单/命令行解释器（grub.cfg 的执行者） |

**引导链**：`boot.img`(512B MBR) → `core.img`(diskboot+ lzma 压缩内核) → 解压出
GRUB 内核 → 加载 `normal.mod` → 读 `grub.cfg` → 执行 `multiboot /boot/kernel.elf`。

## Multiboot 1 协议（我们用的）

内核 ELF 前 8K 内、4 字节对齐处放一个头：

```
magic     = 0x1BADB002
flags     = 0x3      ← 见下方大坑
checksum  = -(magic+flags)
```

GRUB 校验 checksum 后：把 ELF 的 LOAD 段搬到物理地址（我们链到 **1MiB**），
跳到入口，同时给内核两个寄存器：

- `eax = 0x2BADB002`（魔数，证明是 multiboot 引导的）
- `ebx = multiboot 信息结构物理地址`（flags/mem_lower/mem_upper/E820 mmap/…）

信息结构定义见 `stages/s2-kernel-core/kernel.h`，S1 的 kernel.c 演示了解析。

### ⚠️ 实测踩坑：GRUB2 对 flags 极其挑剔

GRUB2 的 multiboot1 加载器**只接受** `flags = 0x1(页对齐) | 0x2(内存信息) | 0x10000(AOUT_KLUDGE)`。
想要 E820 内存映射而把 bit6(0x40) 也置上？直接拒载：

```
error: unsupported flag: 0x43.
error: you need to load the kernel first.
```

而且屏幕报错在无头 QEMU 里看不见，表现就是"串口一片安静、机器傻等"。
正确姿势：头用 `0x3`，信息结构里的 mmap **GRUB 在 PC 上总会给**，内核看信息结构
flags 的 bit6 就行（`stages/s2-kernel-core/mm.c`）。

调试无头引导报错的利器（本仓库用过）：

```bash
# 抓 GRUB 的 VGA 报错画面
(sleep 5; echo "screendump /tmp/vga.ppm"; sleep 1; echo quit) | \
  qemu-system-i386 -cdrom build/boot.iso -display none -monitor stdio
```

## ISO 里有什么

`grub-mkrescue` 产出混合 ISO（BIOS El Torito + EFI）：

```
boot.iso
├── boot/kernel.elf          ← 我们 copy 进去的内核
└── boot/grub/grub.cfg       ← set timeout=0 直接引导
└── boot/grub/i386-pc/*.mod  ← 全部 GRUB 模块（所以镜像有十几 MB）
```

## 学习任务清单（读源码验证）

- [ ] `grub-core/boot/i386/pc/boot.S`：MBR 512B 怎么把自己挪到 0x2000 再继续？
- [ ] `grub-core/loader/i386/pc/multiboot_mbi.c`：flags 校验逻辑（找到 0x43 被拒的那行）；
      信息结构（MBI）是怎么在内存里拼出来的
- [ ] `grub-core/kern/main.c` + `i386/pc/startup.S`：GRUB 内核自己的入口与保护模式切换
- [ ] 用 `grub-mkimage` 手工做一个最小 core.img，理解模块打包
