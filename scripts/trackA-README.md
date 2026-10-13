# S4 Track A —— 真栈 Xfce 复现方案（脚本化路线）

目标：upstream/linux 裁剪编译 + BusyBox rootfs + 逐模块编译 Xfce 核心，
QEMU 图形启动完整桌面。全部步骤可脚本化、可复现。

## 为什么还没跑通
需要宿主安装大量开发依赖（libgtk-3-dev / libglib2.0-dev / XFCE 各模块的
构建链，约 1-2GB apt 包），以及内核+各模块累计 1-2 小时编译。建议在
网络与时间充裕时按下列步骤执行（每步产物落盘可断点续跑）。

## 步骤
1. 依赖：sudo apt install build-essential libgtk-3-dev libglib2.0-dev \
   libdbus-1-dev libdbus-glib-1-dev libx11-dev libxfce4ui-1-dev 等
   （或直接 buildroot：BR2_PACKAGE_XFCE 走整包，产物更可复现）
2. 内核：make tinyconfig + 启用 virtio/9p/PCI/framebuffer/Ext4 → bzImage
3. rootfs：BusyBox 静态编译 + init 脚本（挂 /proc /sys，起 dbus-daemon）
4. Xfce 依赖序逐模块编译入库：
   libxfce4util → xfconf → libxfce4ui → garcon/exo → xfwm4/xfdesktop/
   xfce4-panel/xfce4-session/xfce4-settings/xfce4-appfinder
5. QEMU：-kernel bzImage -append 'root=/dev/vda' -drive rootfs -display sdl
   → 登录 startx → 完整 Xfce 桌面；录像/截图入库 docs/

上游源码已在 upstream/linux 与 upstream/xfce/*（见 MANIFEST.md）。
