# S4 —— Track B：自研内核 GUI（WIP 一项遗留）

已实现并实测：VBE/Dispi+PCI LFB、图元/字体、窗口系统、鼠标点击聚焦、
多 shell 窗口（ESC/spawn）、应用窗（execve 绑定）、calc/clock/notepad。
验收：串口同步注入 + pmemsave 三帧像素断言 + 串口标记。

**遗留（低概率偶发）**：ESC spawn 的 shell 偶发在首次调度后从
tasks[] 区域取指（err=5 va≈0x11acc4，ring3 读 supervisor 页）——
spawn 路径上下文/栈几何待 GDB 硬件断点攻坚；重试可恢复。
（已修根因：保留区漏堆、页表帧 PDE1 盲区、低区 free 不拉 hint、
pd 走普通分配器落用户窗口盲区。）
