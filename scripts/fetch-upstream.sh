#!/usr/bin/env bash
# ============================================================
# fetch-upstream.sh —— 复现 MANIFEST.md 里的上游源码树
#
# 用法:
#   scripts/fetch-upstream.sh           拉取全部（已存在则跳过）
#   scripts/fetch-upstream.sh --check   校验本地 HEAD 与清单一致
#   scripts/fetch-upstream.sh <名字>    只拉某一个（linux/grub/xfce/模块名）
#
# 特性: 浅克隆省流量；镜像优先、官方回退（实测优先，与 github-accel 体系配合）；
#       断点重试 3 次；--check 逐项比对 HEAD。
# ============================================================
set -uo pipefail

cd "$(dirname "$0")/.."
ROOT="$PWD"
UP="$ROOT/upstream"

# 名字|github仓库|官方源(镜像都挂时用)
LINUX_SHA="24a19b953e2a3d0ea5a8c4951a0ab8f32720c907"
GRUB_SHA="b07cc37ca64fe99260884e7325abc333374e098b"
MIRROR="https://gh-proxy.com/https://github.com"   # ghdl 同款镜像，可被环境变量覆盖

REPOS=(
  "linux|torvalds/linux|"
  "grub|rhboot/grub|https://git.savannah.gnu.org/git/grub.git"
  "xfce/libxfce4util|xfce-mirror/libxfce4util|https://gitlab.xfce.org/xfce/libxfce4util.git"
  "xfce/xfconf|xfce-mirror/xfconf|https://gitlab.xfce.org/xfce/xfconf.git"
  "xfce/libxfce4ui|xfce-mirror/libxfce4ui|https://gitlab.xfce.org/xfce/libxfce4ui.git"
  "xfce/garcon|xfce-mirror/garcon|https://gitlab.xfce.org/xfce/garcon.git"
  "xfce/exo|xfce-mirror/exo|https://gitlab.xfce.org/xfce/exo.git"
  "xfce/thunar|xfce-mirror/thunar|https://gitlab.xfce.org/xfce/thunar.git"
  "xfce/xfwm4|xfce-mirror/xfwm4|https://gitlab.xfce.org/xfce/xfwm4.git"
  "xfce/xfdesktop|xfce-mirror/xfdesktop|https://gitlab.xfce.org/xfce/xfdesktop.git"
  "xfce/xfce4-panel|xfce-mirror/xfce4-panel|https://gitlab.xfce.org/xfce/xfce4-panel.git"
  "xfce/xfce4-session|xfce-mirror/xfce4-session|https://gitlab.xfce.org/xfce/xfce4-session.git"
  "xfce/xfce4-settings|xfce-mirror/xfce4-settings|https://gitlab.xfce.org/xfce/xfce4-settings.git"
  "xfce/xfce4-appfinder|xfce-mirror/xfce4-appfinder|https://gitlab.xfce.org/xfce/xfce4-appfinder.git"
  "xfce/tumbler|xfce-mirror/tumbler|https://gitlab.xfce.org/xfce/tumbler.git"
)

only="${1:-}"
check=0
[ "${only:-}" = "--check" ] && { check=1; only=""; }

clone_one() {
  local name="$1" repo="$2" official="$3"
  local dir="$UP/$name"
  if [ -d "$dir/.git" ]; then
    echo "== $name 已存在 ($(git -C "$dir" rev-parse --short HEAD))"
    return 0
  fi
  mkdir -p "$UP/$(dirname "$name")"
  local urls=("$MIRROR/https://github.com/$repo" "https://github.com/$repo")
  [ -n "$official" ] && urls+=("$official")
  for u in "${urls[@]}"; do
    for attempt in 1 2 3; do
      echo "== clone $name (尝试 $attempt): $u"
      if GIT_TERMINAL_PROMPT=0 git clone --depth 1 -q "$u" "$dir" 2>&1 | sed 's/^/   /'; then
        [ -d "$dir/.git" ] && { echo "   OK: $(git -C "$dir" rev-parse --short HEAD)"; return 0; }
      fi
      rm -rf "$dir"; sleep $((attempt * 3))
    done
  done
  echo "!! $name 拉取失败"; return 1
}

fail=0
for entry in "${REPOS[@]}"; do
  IFS='|' read -r name repo official <<< "$entry"
  [ -n "$only" ] && [ "$name" != "$only" ] && [ "$(basename "$name")" != "$only" ] && continue
  if [ "$check" = 1 ]; then
    dir="$UP/$name"
    if [ ! -d "$dir/.git" ]; then echo "MISSING  $name"; fail=1; continue; fi
    head_sha="$(git -C "$dir" rev-parse HEAD)"
    pin=""
    case "$name" in
      linux) pin="$LINUX_SHA" ;;
      grub)  pin="$GRUB_SHA" ;;
    esac
    if [ -n "$pin" ]; then
      if [ "$head_sha" = "$pin" ]; then echo "OK       $name $head_sha"
      else echo "DRIFT    $name HEAD=$head_sha 清单=$pin"; fail=1; fi
    else
      echo "LOCAL    $name $head_sha（xfce 模块以克隆日 HEAD 为准，升级请更新 MANIFEST.md）"
    fi
  else
    clone_one "$name" "$repo" "$official" || fail=1
  fi
done

[ "$fail" = 0 ] && echo "== 全部完成 ==" || { echo "== 有失败项 =="; exit 1; }
