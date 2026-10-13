#!/usr/bin/env bash
# ============================================================
# gen-lesson.sh —— 课程生成/重置器（幂等）
#
# 用法:
#   tools/gen-lesson.sh            生成/重置全部课程
#   tools/gen-lesson.sh L09        只重置某一课（会覆盖你的解答！）
#
# 规则: stages/ 是唯一事实源。每课 = 阶段工程 + 课程模板(门控 main/Makefile)
#       + progress.h + 本课桩文件 + 任务书。stages/ 修了 bug 重跑本脚本即可。
# ============================================================
set -euo pipefail
cd "$(dirname "$0")/../.."          # 仓库根

LESSONS=(
  "01 boot-bios      s0"
  "02 serial         s0"
  "03 hexout         s0"
  "04 multiboot      s1"
  "05 grub-iso       s1"
  "06 meminfo        s1"
  "07 e820           s1"
  "08 console        s2"
  "09 gdt            s2"
  "10 idt            s2"
  "11 pic-pit        s2"
  "12 pmm            s2"
  "13 heap           s2"
  "14 task-sched     s2"
  "15 ramfs          s2"
  "16 syscall        s2"
  "17 ring3          s2"
  "18 kbd            s2"
  "19 capstone       s2"
)

STAGE_DIR() {
  case "$1" in
    s0) echo stages/s0-mbr ;;
    s1) echo stages/s1-grub ;;
    s2) echo stages/s2-kernel-core ;;
  esac
}

gen_one() {
  local num="$1" title="$2" st="$3"
  local dir="course/L${num}-${title}"
  local stage; stage="$(STAGE_DIR "$st")"
  local lesson="L${num}"

  rm -rf "$dir"
  mkdir -p "$dir"
  cp -r "$stage/." "$dir/"
  rm -rf "$dir/build"

  case "$st" in
    s0)
      cp course/tools/template/s0-Makefile "$dir/Makefile"
      printf '#define LESSON %s\n' "$((10#$num))" > "$dir/progress.h"
      ;;
    s1)
      cp course/tools/template/s1-kernel.c "$dir/kernel.c"
      cp course/tools/template/s1-Makefile "$dir/Makefile"
      printf '#define LESSON %s\n' "$((10#$num))" > "$dir/progress.h"
      ;;
    s2)
      cp course/tools/template/s2-main.c   "$dir/main.c"
      cp course/tools/template/s2-Makefile "$dir/Makefile"
      printf '#define LESSON %s\n' "$((10#$num))" > "$dir/progress.h"
      ;;
  esac

  # 本课桩文件（保留相对路径覆盖，例如 L05 的 iso/boot/grub/grub.cfg）
  if [ -d "course/tools/stubs/$lesson" ]; then
    cp -a "course/tools/stubs/$lesson/." "$dir/"
  fi

  # 任务书
  local tb="course/tools/lessons/L${num}.md"
  [ -f "$tb" ] && cp "$tb" "$dir/README.md" || { echo "!! 缺任务书: $tb"; exit 1; }

  echo "gen  $dir"
}

want="${1:-all}"
for row in "${LESSONS[@]}"; do
  set -- $row
  if [ "$want" = all ] || [ "L$1" = "$want" ]; then
    gen_one "$1" "$2" "$3"
  fi
done
echo "== 完成 =="
