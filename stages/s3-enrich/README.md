# S3 —— 丰富系统（规划占位）

本阶段从 `stages/s2-kernel-core` 复制起步（`cp -r`），不推倒重来。

任务清单与 upstream 对照表见根目录 [ROADMAP.md](../../ROADMAP.md) 的 S3 节和
[docs/20-kernel.md](../../docs/20-kernel.md)。

推进顺序：分页与虚存 → COW fork → ELF 加载 → VFS/fd 表 → Ext2 只读 → 系统调用表 + 用户态 shell。

每完成一项：`make test` 增加对应串口断言 → 提交。
