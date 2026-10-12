# 30 · Xfce 架构与结合路线（S4）

`upstream/xfce/*`（12 核心模块，SHA 见 MANIFEST.md）。

## 模块分层（自下而上）

```
glib2 / GTK3            ← 一切的地基（宿主库，非 Xfce 项目）
├── libxfce4util        工具函数、生命周期
├── xfconf              配置系统：D-Bus 守护 xfconfd + 客户端绑定
├── libxfce4ui          UI 部件（对话框/菜单扩展），接 GTK
├── garcon              XDG 菜单实现
├── exo                 组件扩展（图标视图等，Thunar 依赖）
├── tumbler             缩略图服务（D-Bus）
└── ── 桌面会话层 ──
    ├── xfwm4           窗口管理器（自己实现的合成器可选）
    ├── xfce4-panel     面板（插件体系）
    ├── xfdesktop       桌面（图标/壁纸/右键菜单）
    ├── xfce4-session   会话管理（登录→启动各组件→注销协议）
    ├── xfce4-settings  设置（显示/键盘/外观，xfconf 后端）
    └── xfce4-appfinder 应用启动器
```

**关键认识**：Xfce 是一组普通 POSIX/GTK 程序——它需要 Linux 内核 + X11/Wayland 栈 +
D-Bus + 一堆系统库。**它不会直接跑在自研内核上**。所以 S4 是双轨：

## Track A —— 真栈复现（可复现脚本化，先做）

用 upstream 源码在 QEMU 里拼出真·Xfce 桌面，全流程进 Makefile：

1. 内核：`upstream/linux` 裁剪配置（tinyconfig + 9P/virtio/PCI/framebuffer/Ext4）
2. rootfs：BusyBox + 编译产物，init 脚本起 D-Bus → xfce4-session
3. 逐模块 `./configure && make`（依赖序照上表自下而上），产物装进 rootfs
4. `qemu-system-x86_64 -display sdl/gtk` 出完整桌面；录像/截图入库到 docs/

产出价值：理解"一个桌面发行版"从内核到壁纸的每一层。

## Track B —— 自研内核 GUI（对照学习）

在 S3 内核上长出自己的图形栈（参考 linux011-course day37-40 的窗口 API 经验）：

1. framebuffer：QEMU 标准 VGA LFB（PCI BAR 探测），双缓冲
2. 字体/绘制：8x16 位图字体起步，脏矩形刷新
3. 窗口系统：内核窗口表 + 焦点 + 事件队列；窗口 API 系统调用化
4. 用户态演示：终端窗、时钟窗、文件浏览窗（对齐 Xfce 的桌面范式）

**"结合"的落点**：Track B 的窗口演示与 Track A 的 Xfce 截图并排对照，
写《我们的桌面 vs Xfce》差距报告（进程模型/合成/配置系统/多用户），反哺 S5+ 规划。
