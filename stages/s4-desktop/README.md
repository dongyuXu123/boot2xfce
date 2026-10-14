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

## 内核纯净原则（架构定调）

内核只提供通用机制：窗口/画布/事件服务（win.c）、绘制与事件系统调用
（sys25-29：wgfx/wrect/wtext/wevent/yield）、进程隔离（每任务页目录、
T_EMBRYO、用户指针校验）。calc/clock/notepad 是纯用户 ELF——按钮布局、
表盘、编辑缓冲全部在用户态，内核零应用知识。

## 隔离与调度正确性（第三轮修复，2026-10-14）

1. **page_dir_create 只继承 PDE0**：曾把 boot_pd 全部内核区（恒等映射/
   LFB/画布别名）以共享页表继承给用户任务，execve 的 clear_user 把它们
   当任务私产整张释放 → boot_pd 的 fb/画布映射随机蒸发（画布内容"灵异
   消失"、err=2 写 0xfd000000 的总根因）。clear_user/copy_space 范围
   同步收敛到用户窗口（PDE1-4）。
2. **exit 必须切走**：sys(3) 标完 T_DEAD 就 break 返回，死任务霸占 CPU，
   其内核栈被 reap 的 kfree 收回而 TSS esp0 仍指着它——此后每个中断往
   "已释放的栈"压帧，随机腐蚀任何新主。现 exit 即 schedule 且不返回。
3. **每任务 if_on**：switch_to 不携带 EFLAGS，协作切换都发生在 int80
   门内（IF=0），关中断态沿调度链传染（gfx 饿死在 memset 半路、时钟全
   停）。切回点按 task->if_on 恢复自己的 IF；PIT 抢占从帧里取被中断者
   的真实 IF。
4. **kbd/mouse 环形缓冲加锁**：IRQ 压入与任务弹出交错会丢键（偶发回车
   失灵）。pop 侧关中断临界区。
5. **fb_rect 裁剪 + rep stosl、画布 blit rep movsl**：TCG 下全屏重绘
   曾需秒级，clock 每秒重绘把显示管线堵成永久撕裂态。

已知问题：clock 窗能创建但首绘/聚焦偶发不就位（fork/exec 时序 flake +
应用首绘），另行攻坚。
