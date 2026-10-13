#!/usr/bin/env python3
"""check-fb.py —— S4 图形验收：pmemsave 转储的 LFB(640x480x32) 像素断言。
32bpp 小端：字节序 B G R x。用法: check-fb.py fb.bin
"""
import sys
STRIDE, W, H = 2560, 640, 480
def load(path):
    fb = open(path, 'rb').read()
    return fb if len(fb) >= STRIDE * H else None

def main():
    global fb
    got = None
    for p in sys.argv[1:]:                # 连拍多帧：重绘进行中可能抓到半帧
        fb = load(p)
        if fb: got = p
    assert got, 'no usable fb dump'
    def px(x, y):
        o = y * STRIDE + x * 4
        return (fb[o+2], fb[o+1], fb[o])          # R G B
    ok = True
    def check(name, x, y, rgb):
        nonlocal ok
        got = px(x, y)
        if all(abs(a-b) <= 12 for a, b in zip(got, rgb)):
            print(f'[ok] {name} @({x},{y}) {got}')
        else:
            print(f'[FAIL] {name} @({x},{y}) got={got} want={rgb}')
            ok = False
    check('desktop bg',   600, 440, (0x1E, 0x3A, 0x5F))
    check('taskbar',      320, 475, (0x14, 0x20, 0x2E))
    check('start btn',      8, 464, (0x4C, 0xD1, 0x37))
    check('win0 bg',      150, 120, (0xF4, 0xF4, 0xF4))
    check('win1 bg',      400, 120, (0xF4, 0xF4, 0xF4))
    check('win2 bg',      560, 180, (0xF4, 0xF4, 0xF4))
    check('win1 titlebar', 300,  56, (0x57, 0x60, 0x6F))
    check('win2 titlebar', 560,  56, (0x2E, 0x86, 0xDE))
    print('[PASS] screen' if ok else '[FAIL] screen')
    sys.exit(0 if ok else 1)
main()
