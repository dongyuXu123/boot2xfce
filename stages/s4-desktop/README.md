# S4 —— Track B：自研内核 GUI（WIP：fork×低区池交互有 1 个未解 bug）

已实现并验证的部分：VBE/Dispi 模式设置（16 位数据口）、PCI 探测 LFB 基址、
双缓冲→直写 LFB 的图元（像素/矩形/8x8 字体/文字）、窗口系统数据结构、
shell `gfx` 命令 → SYS 17 → gfx 内核任务接管屏幕与键盘。

**当前阻断**：init 的 fork（S3-M3 在 S3 全绿）在 S4 环境下于
`page_dir_create` 后触发 UD 异常（eip≈3，ring0 esp 落在低区池帧内）。
已定位到"fork 与低区池/保留区几何的相互作用"，复现线索：
- [f pid=4] int14 先行，随后 int6@eip=3，无 panic；
- 排除：gfx 任务（去掉仍复现）、page_copy_space 逻辑（与 S3 全同 diff）、
  task_clone（与 S3 全同 diff）；
- 唯一逻辑差异：boot_pd 走低区分配 + 低区池 [0x120000,0x200000)。
- 历史根因（已修）：保留区漏掉堆 → boot_pd 压堆头；
  页表帧落 PDE1 盲区 → 低区分配器；低区帧 free 不拉 hint。

下一步建议：GDB 硬件断点跟 `page_dir_create` 返回后的第一次调度；
或先让 Track A（真栈 Xfce）落地，Track B 的窗口系统在独立分支重排
地址空间布局（内核高半区 0xC0000000，彻底消除低区池概念）。
