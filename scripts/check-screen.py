#!/usr/bin/env python3
"""check-screen.py —— S4 图形验收：QEMU screendump(P PM) 像素断言
断言四件事：桌面背景、任务栏、绿色启动钮、焦点窗口金色边框。
用法: check-screen.py screen.ppm
"""
import sys

def main():
    ppm = open(sys.argv[1], 'rb').read()
    # 头部可能含 # 注释行（QEMU 会写）——逐行跳过到 255 那行
    pos, lines = 0, []
    while len(lines) < 3:
        nl = ppm.index(b'\n', pos)
        line = ppm[pos:nl]
        pos = nl + 1
        if line.startswith(b'#') or not line.strip():
            continue
        lines.append(line)
    assert lines[0] == b'P6', 'not a P6 ppm'
    w, h = map(int, lines[1].split())
    raw = ppm[pos:]
    def px(x, y):
        o = (y * w + x) * 3
        return (raw[o], raw[o+1], raw[o+2])
    def near(a, b, tol=12):
        return all(abs(x-y) <= tol for x, y in zip(a, b))

    ok = True
    def check(name, x, y, rgb):
        nonlocal ok
        got = px(x, y)
        if not near(got, rgb):
            print(f'[FAIL] {name} @({x},{y}) got={got} want={rgb}')
            ok = False
        else:
            print(f'[ok] {name} @({x},{y}) {got}')

    check('desktop bg', 600, 200, (0x1E, 0x3A, 0x5F))
    check('taskbar',    320, 470, (0x14, 0x20, 0x2E))
    check('start btn',   30, 464, (0x4C, 0xD1, 0x37))
    check('focus border', 40, 100, (0xF9, 0xCA, 0x24))
    check('win bg',     150, 120, (0xEE, 0xEE, 0xEE))
    print('[PASS] screen' if ok else '[FAIL] screen')
    sys.exit(0 if ok else 1)

main()
