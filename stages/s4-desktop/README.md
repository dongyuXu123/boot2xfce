# S4 —— Track B：自研内核 GUI（全部功能实测通过）

已实现并实测：VBE/Dispi+PCI LFB、图元/字体、窗口系统、鼠标点击聚焦、
多 shell 窗口（ESC/spawn）、应用窗（execve 绑定）、calc/clock/notepad。
验收：`make test`（scripts/s4-test.py）——串口同步注入 + **screendump
真实扫描输出断言**（VNC 前端）+ pmemsave 显存断言 + 键鼠交互像素断言，
30 项全绿。screendump 抓的是 QEMU 实际显示面：dispi 模式切换若未真实
生效（显存对、屏幕不对）在此暴露，pmemsave 无法替代。

## gfx 三大根因（对照 QEMU hw/display/vga.c 定位）

1. **dispi 索引口必须 16 位写**：QEMU `vbe_portio_list_x86` 对 0x1CE/0x1CF
   只登记 size=2 访问，`outb` 写索引被静默丢弃 → vbe_index 恒 0 →
   模式切换从未生效（显存内容正确、屏幕停留文本模式 720x400）。
   Linux bochs 驱动（drivers/gpu/drm/tiny/bochs.c）同样 outw。
2. **gfx 下 stdin 必须路由窗口行规程**：tty_read_impl 在 gfx 激活时
   转 tty_read_window（win.c wt[]）；否则 shell 与 gfx_task 抢 kbd_pop，
   且等的 line_ready 永不置位——窗口回车失灵、命令不执行。
3. **绘制单线程化**：帧缓冲只允许 gfx_task 绘制；spawn/execve 的系统
   调用上下文只许 cli 临界区改 wins[] + 置 gfx_dirty（否则抢占交错出
   撕裂帧）。gfx_task 消费 gfx_dirty 实现输出驱动刷新。

## T_EMBRYO（抢占安全创建）

IRQ0 的 schedule() 是抢占点。任务创建期间（kmalloc 栈/页目录/iret 帧
未就绪）必须对调度器不可见：task_create/task_clone 以 T_EMBRYO 构建，
镜像与窗口全就绪后由 spawn/fork/init 置 T_READY。历史症状：半成品任务
被 tick 调度，蹦床从回收内存弹垃圾 iret 帧 → 用户态取指内核地址
（PAGE FAULT err=5 va≈0x11xxxx，偶发）。

其余曾修根因：保留区漏堆、页表帧 PDE1 盲区、低区 free 不拉 hint、
pd 走普通分配器落用户窗口盲区、grub.cfg 勿加 gfxpayload/insmod vbe
（文本引导 + 内核自切模式是已验证路径，加 vbe 反而引导卡死）。
