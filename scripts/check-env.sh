#!/usr/bin/env bash
# 记录构建环境版本到 build/env.txt —— 可复现性的"指纹"
set -e
cd "$(dirname "$0")/.."
mkdir -p build
{
  echo "date: $(date -Is)"
  echo "host: $(uname -sr) $(uname -m)"
  gcc --version | head -1
  echo "ld: $(ld --version | head -1)"
  nasm -v
  qemu-system-i386 --version | head -1
  grub-mkrescue --version | head -1
  xorriso --version 2>/dev/null | head -1
} | tee build/env.txt
