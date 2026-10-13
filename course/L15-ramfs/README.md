# L15 · 内存盘 ramfs

**目标**：第一个文件系统——内存里的，但 create/read/write 语义是真的。
**你亲手写**（`ramfs.c` 的三件套；name_eq/lookup/list 给定）：
- `ramfs_init`：memset 整个文件表（想想为什么一次 memset 就够）。
- `ramfs_write(name, buf, len)`：len 超 FILE_MAX 截断 → lookup 找已有
  文件，没有就找空槽创建（名字逐字符拷，最多 15 字符，留 \0）→
  表满返回 -1 → 拷数据、记 size，返回写入长度。
- `ramfs_read(name, buf, max)`：找不到返回 -1；拷 min(size,max)，
  返回实际长度。

**背景**：固定 8 文件 × 512B，简陋但诚实——S3 会把它换成 vnode 抽象
+ Ext2，那时你会怀念它的简单。
**验收**：写 "hello,ramfs!\n"（13B）读回逐字节一致 → `S2: ramfs OK`。
**参考答案**：`stages/s2-kernel-core/ramfs.c`。
