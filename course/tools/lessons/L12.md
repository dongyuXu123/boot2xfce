# L12 · 物理页位图 PMM

**目标**：把 E820 普查结果变成能分配的 4KiB 页——内存管理的地基。
**你亲手写**（`mm.c` 的 PMM 四件套；堆是给定成果，L13 才轮到你）：
1. `pmm_init(mb)`：清位图 → 遍历 mmap（type==1 才可用；base 截 ≥1MiB、
   4K 取整；lim 截 PMM_LIMIT）→ 数出 `pmm_total` → 保留三块固定区：
   内核镜像 [1M, kernel_end]、堆 [HEAP_START,HEAP_END)、用户区
   [USER_BASE, USER_BASE+1M)。
2. `pmm_total_frames` / `pmm_alloc_frame`（从 `pmm_hint` 起找首个 0 位）/
   `pmm_free_frame`（清位 + 拉回 hint）。

**验收（实测）**：kmain 连发两页必须不同且 4K 对齐；释放 a 后再发必须
拿回 a——`S2: pmm OK` 才算过。
**参考答案**：`stages/s2-kernel-core/mm.c`。
