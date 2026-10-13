# L08 · 双路 console

**目标**：内核的话要同时让人看见（VGA）和让测试听见（串口）——所有后续
课程的地基。从这里起进入 S2 内核课。
**你亲手写**（`console.c` 全部 TODO）：
- `serial_init` / `serial_putc`（32 位 C 版，L02 你用汇编写过一遍了）
- `vga_putc`：字符写 `0xB8000`（属性 0x07=黑底白字），`\n` 换行，
  80 列满换行，25 行满回卷第 0 行
- `console_init`：serial_init + 清屏（0x0720 空格）+ 光标归位(row=1)
- `print`（逐字符双路）、`print_hex`（0x 前缀+8 位小写）、`print_dec`（从个位
  往回填缓冲；想想 v=0 时要打 "0"）

**验收**：`make test`——kmain 用你的 print 打 `S2: console OK`，
桩是哑的所以必红；写完即绿。
**参考答案**：`stages/s2-kernel-core/console.c`。
