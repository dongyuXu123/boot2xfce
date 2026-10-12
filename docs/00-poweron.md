# 00 · 加电：从复位向量到 0x7C00（S0）

## x86 加电后发生了什么

1. **复位**：CPU 引脚 RESET 拉起后，CPU 从 `0xFFFF0`（CS=0xF000, IP=0xFFF0）开始执行，
   这里是 BIOS EPROM 的入口。此时 CPU 处于**实模式**（16 位，1MB 地址空间，无保护）。
2. **POST**：BIOS 自检（内存/显卡/键盘…），建立中断向量表 IVT（实模式中断 `int 10h` 等）。
3. **找启动设备**：按启动顺序逐个设备读第一个扇区（512B），检查最后两字节是否
   `0x55 0xAA`（小端 `dw 0xAA55`）——是则判定为可引导。
4. **交权**：把这 512 字节加载到物理地址 `0x7C00`，`jmp 0x7C00`——**从此 CPU 归我们管**。

> 为什么是 0x7C00？IBM PC 时代的历史选择：给 BIOS 数据区和中断向量留出低地址空间，
> 同时把栈和引导程序放进 32KB 以内。约定俗成沿用至今。

## S0 代码要点（stages/s0-mbr/mbr.asm）

- `[ORG 0x7C00]`：告诉汇编器所有标号相对 0x7C00 定位；
- 段寄存器清零 + `sp=0x7C00`（栈向下长，正好在代码下方，不冲突）；
- 输出走两条路：
  - **BIOS `int 10h`（AH=0Eh）** → VGA，给人看；
  - **直接 `out 0x3F8`** → COM1 串口，给测试断言用（QEMU `-serial` 抓取）。
  串口初始化 = 设波特率除数（115200）+ 8N1 + FIFO，全是对端口的原始 IO——
  因为这时候"操作系统"就是我们自己这 512 字节。
- 510 字节处补零，511~510 写 `0xAA55`。

## 实测方法

```bash
make -C stages/s0-mbr test
# 内部：nasm → 512B 引导扇区 → dd 进 1.44MB 软盘镜像
#       qemu-system-i386 -drive if=floppy -display none -serial file:...
#       grep 串口日志断言 "S0: power-on OK"
```

## 下一步（S1）

MBR 时代已死——真实系统引导扇区属于 GRUB。S1 起把引导权交给 GRUB，
我们按 **Multiboot 协议**提供内核，见 `docs/10-grub.md`。
