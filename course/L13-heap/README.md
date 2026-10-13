# L13 · 内核堆 kmalloc / kfree

**目标**：任意大小的内核内存分配——任务栈、PCB 全靠它。
**你亲手写**（`mm.c` 的堆三件套；PMM 用你 L12 的成果）：
- `heap_init`：把 [HEAP_START, HEAP_END) 初始化成一个巨大空闲块
  （12 字节块头：size/used/next）。
- `kmalloc(n)`：n 4K 对齐……不对，是 4 **字节**对齐；首次适应找
  !used 且 size≥n 的块；剩余够"块头+8"就分裂出后继空闲块；
  标 used，返回块头之后的地址。
- `kfree(p)`：p 退回 12 字节找到块头，标 !used，然后**向后合并**相邻
  空闲块（合并后 size 变大，所以要继续检查新邻居——循环别写成 if）。

**验收（实测）**：p1=kmalloc(100)、p2=kmalloc(200)、kfree(p2)、
q=kmalloc(150) 必须 q==p2（分裂边界错了就复用不上）。
**参考答案**：`stages/s2-kernel-core/mm.c`。
