# L07 · 遍历 E820 内存映射

**目标**：拿到操作系统最贵的数据——哪段内存能用。
**你亲手写**（`kernel.c` 的 L07 TODO 区）：
1. 先打 `S1: memory map (E820):`
2. 遍历每条记录，打 `  mmap base=<hex> len=<hex> type=<dec>`（最多 6 条）
3. 最后打 `S1: mmap entries=<总数>`

**⚠️ 全课程第一陷阱**：记录的真实长度在 `e->size` 字段（且**不含自身**），
下一条记录的地址 = 当前地址 + `e->size + 4`——照 struct 大小加步进必炸。
另外遍历终点是 `mmap_addr + mmap_length`，不是数条数。

**背景**：E820 是 BIOS 时代传下来的内存普查表；type=1 可用、2 保留、
3/4 ACPI。`flags` bit6=1 表示表存在。L12 的物理内存管理就吃这张表。

**验收**：`make test` 断言 `S1: mmap entries=` 和逐条 `S1: mmap base=`。
**参考答案**：`stages/s1-grub/kernel.c`。
