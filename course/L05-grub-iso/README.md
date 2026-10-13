# L05 · grub.cfg 与 ISO 制作

**目标**：亲手把内核做成可引导光盘镜像，理解 GRUB 怎么找到"该引导谁"。
**你亲手写**：
1. `iso/boot/grub/grub.cfg`：`set timeout=0` 直接引导 + 一个
   `menuentry "boot2xfce s1" { multiboot /boot/kernel.elf
     boot }`（路径是 ISO 内路径！）。
2. `Makefile` 的 `$(B)/boot.iso` 规则：建 `build/iso/boot/grub/` →
   拷 kernel.elf 和 grub.cfg → `grub-mkrescue -o $@ build/iso`。

**背景**：grub-mkrescue 产出 BIOS+EFI 混合 ISO，里面塞了全部 GRUB 模块
（所以有十几 MB）；GRUB 起来后按配置里的 `multiboot` 命令加载我们的 ELF。

**验收**：`make test`——这次从空 grub.cfg 开始，引导链全是你拼出来的。
**调试彩蛋**：引导失败屏幕没信息？任务书外送一招：
`(sleep 5; echo "screendump /tmp/vga.ppm"; echo quit) | qemu-system-i386 -cdrom build/boot.iso -display none -monitor stdio`，
把 PPM 转成 PNG 就能看到 GRUB 的报错原文。
**参考答案**：`stages/s1-grub/`。
