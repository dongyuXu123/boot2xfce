#!/usr/bin/env bash
# ============================================================
# verify.sh —— 教师视角全课程体检（不动 course/ 里的文件）
# 每课两问：
#   1) 桩状态  make test 必须 FAIL（红 = 这课确实有活干）
#   2) 答案态  注入参考答案后 make test 必须 PASS（绿 = 任务书可解）
# 用法: tools/verify.sh [LNN ...]      默认全部；-j 并行数默认 4
# ============================================================
set -u
cd "$(dirname "$0")/../.."
ROOT="$PWD"
TMP="$(mktemp -d /tmp/b2x-verify.XXXXXX)"
JOBS=4

run_one() {
  local num="$1"
  local dir
  dir=$(ls -d "$ROOT"/course/L${num}-* 2>/dev/null | head -1)
  [ -n "$dir" ] || { echo "L$num SKIP(no dir)"; return; }
  local name; name=$(basename "$dir")
  local work="$TMP/$name"

  # ---- 阶段1: 桩必红（L19 是全通状态的毕业样例，桩态即绿，跳过） ----
  if [ "$num" = 19 ]; then
    echo "L19 毕业样例     ✓（全通状态，无桩）"
  elif make -s -C "$dir" clean > /dev/null 2>&1 && make -s -C "$dir" test > /tmp/v1-$num.log 2>&1
  then
    echo "L$num 桩态应红却绿  ✗"; return
  else
    echo "L$num 桩态红       ✓"
  fi

  # ---- 阶段2: 答案必绿 ----
  cp -r "$dir" "$work"
  case "$num" in
    01|02) cp "$ROOT/stages/s0-mbr/mbr.asm" "$work/" ;;
    03)    cp "$ROOT/course/tools/solutions/L03-mbr.asm" "$work/mbr.asm" ;;
    04|05|06|07)
           cp "$ROOT/stages/s1-grub/boot.asm"  "$work/" 2>/dev/null
           cp "$ROOT/stages/s1-grub/kernel.ld" "$work/" 2>/dev/null
           cp "$ROOT/stages/s1-grub/kernel.c"  "$work/" 2>/dev/null
           cp "$ROOT/stages/s1-grub/Makefile"  "$work/" 2>/dev/null
           cp "$ROOT/stages/s1-grub/iso/boot/grub/grub.cfg" "$work/iso/boot/grub/" ;;
    08)    cp "$ROOT/stages/s2-kernel-core/console.c" "$work/" ;;
    09)    cp "$ROOT/stages/s2-kernel-core/gdt.c"     "$work/" ;;
    10)    cp "$ROOT/stages/s2-kernel-core/idt.c"  "$work/"
           cp "$ROOT/stages/s2-kernel-core/isr.asm" "$work/" ;;
    11)    cp "$ROOT/stages/s2-kernel-core/irq.c"  "$work/" ;;
    12|13) cp "$ROOT/stages/s2-kernel-core/mm.c"   "$work/" ;;
    14)    cp "$ROOT/stages/s2-kernel-core/cpu.asm" "$work/"
           cp "$ROOT/stages/s2-kernel-core/task.c"  "$work/" ;;
    15)    cp "$ROOT/stages/s2-kernel-core/ramfs.c" "$work/" ;;
    16)    cp "$ROOT/stages/s2-kernel-core/syscall.c" "$work/" ;;
    17)    cp "$ROOT/stages/s2-kernel-core/cpu.asm" "$work/"
           cp "$ROOT/stages/s2-kernel-core/user.c"  "$work/" ;;
    18)    cp "$ROOT/stages/s2-kernel-core/irq.c"  "$work/" ;;
    19)    ;;        # 毕业样例：全给定，桩态本身就是绿
  esac
  if make -s -C "$work" clean > /dev/null 2>&1 && make -s -C "$work" test > /tmp/v2-$num.log 2>&1
  then
    echo "L$num 答案绿       ✓"
  else
    echo "L$num 答案态应绿却红 ✗  (日志 /tmp/v2-$num.log)"
  fi
  rm -rf "$work"
}

if [ $# -gt 0 ]; then
  for l in "$@"; do run_one "${l#L}"; done
else
  nums=$(ls course | grep -oE '^L[0-9]+' | tr -d 'L' | sort -n)
  echo "$nums" | xargs -P "$JOBS" -I{} bash -c "$(declare -f run_one); ROOT='$ROOT'; TMP='$TMP'; run_one {}" \
    | sort -t L -k2 -n
fi
echo "== 体检完成（临时目录 $TMP）=="
