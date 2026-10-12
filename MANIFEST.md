# MANIFEST —— 上游参考源码清单（钉版本，可复现）

> `upstream/` 目录体积巨大（仅 Linux 就 ~2.1G），**不入 git**。
> 本文件 + `scripts/fetch-upstream.sh` 保证任何人都能还原出完全一致的源码树。
> 抓取时间：2026-10-12；git 下载自动走本机加速镜像（push 永远走官方）。

| 本地路径 | 上游 | 钉住的 commit | 说明 |
|---|---|---|---|
| `upstream/linux` | github.com/torvalds/linux | `24a19b953e2a3d0ea5a8c4951a0ab8f32720c907` | Linux 7.3-rc7 master |
| `upstream/grub` | git.savannah.gnu.org/git/grub | `b07cc37ca64fe99260884e7325abc333374e098b` | GRUB 2.15+ master |
| `upstream/xfce/libxfce4util` | github.com/xfce-mirror | `53ddad3ac0de5864386ccc4afb66d6bc9bd664d7` | 基础工具库 |
| `upstream/xfce/xfconf` | github.com/xfce-mirror | `b82b0a5e7781a6d391d9fff9f41a8da256ec0f74` | 配置系统(D-Bus) |
| `upstream/xfce/libxfce4ui` | github.com/xfce-mirror | `7ac532af9a5966d232a48e95cbebed4c095efee7` | UI 组件库 |
| `upstream/xfce/garcon` | github.com/xfce-mirror | `f8ae481fba7c6c40cf636b02ad536b20318c4068` | 菜单实现 |
| `upstream/xfce/exo` | github.com/xfce-mirror | `de0e9257cb8a748f5be555e0f95487014d551e9f` | 扩展组件库 |
| `upstream/xfce/thunar` | github.com/xfce-mirror | `b1d2264ccae761e6bd1384fb992c4ea3d4564c44` | 文件管理器 |
| `upstream/xfce/xfwm4` | github.com/xfce-mirror | `30f9597be874604702600099480c82416b77e456` | 窗口管理器 |
| `upstream/xfce/xfdesktop` | github.com/xfce-mirror | `def3bfbed19508e57502bc6ff151950f87c41ca1` | 桌面 |
| `upstream/xfce/xfce4-panel` | github.com/xfce-mirror | `9c3dae60edec6c33dc31a7415b4c5192a2d9a9ce` | 面板 |
| `upstream/xfce/xfce4-session` | github.com/xfce-mirror | `35bc1359fc52842a73450176a6bf5186c4080e50` | 会话管理 |
| `upstream/xfce/xfce4-settings` | github.com/xfce-mirror | `007ea711aa14c4a90f7fcfd1312bfb63c6ec4b1d` | 设置 |
| `upstream/xfce/xfce4-appfinder` | github.com/xfce-mirror | `91f5a35a6867a246cc522ab53ce88adfa1820244` | 应用查找器 |
| `upstream/xfce/tumbler` | github.com/xfce-mirror | `d1df08f4d83867b17dceca54e729926296e48322` | 缩略图服务 |

## 复现方法

```bash
scripts/fetch-upstream.sh          # 按 MANIFEST 全量拉取（浅克隆，带重试）
scripts/fetch-upstream.sh --check  # 校验本地 HEAD 是否与清单一致
```

升级某个上游时：重新克隆后把新 SHA 更新进本文件并提交，保持"清单即事实"。
