#!/usr/bin/env python3
"""s4-test.py —— S4 图形栈端到端验收（串口同步驱动）

与旧盲注入配方的区别：
1. 一切键序在串口标记出现后才注入（不靠固定延时赌时序）
2. 用 -vnc 挂一个真实显示前端：HMP screendump 抓到的是 QEMU 实际
   扫描输出的画面（dispi 模式切换若未生效，这里只会是 720x400 文本）——
   pmemsave 只能证明显存内容，不能证明屏幕真的切了模式
3. 覆盖 gfx 窗口回车链路（历史上 '\\n' 多字符常量损坏让 Enter 永远失效）
4. 断言串口无调试探针回流

用法: s4-test.py <stage目录绝对路径>
"""
import os, socket, subprocess, sys, time

STAGE = os.path.abspath(sys.argv[1])
B = os.path.join(STAGE, 'build')
SERIAL = os.path.join(B, 'serial.log')
ISO = os.path.join(B, 'boot.iso')

# 与 win.c 调色板一致
C_DESKTOP     = (0x1E, 0x3A, 0x5F)
C_TASKBAR     = (0x14, 0x20, 0x2E)
C_START       = (0x4C, 0xD1, 0x37)
C_TITLE_FOC   = (0x2E, 0x86, 0xDE)
C_TITLE_UNFOC = (0x57, 0x60, 0x6F)
C_WIN_BG      = (0xF4, 0xF4, 0xF4)

def say(msg):
    print(msg, flush=True)

class Guest:
    def __init__(self):
        for f in ('serial.log', 'mon.sock', 'scan1.ppm', 'scan2.ppm', 'scan3.ppm',
                  'scan4.ppm', 'scan5.ppm', 'scan6.ppm', 'fb1.bin'):
            p = os.path.join(B, f)
            if os.path.exists(p):
                os.remove(p)
        self.qemu = subprocess.Popen(
            ['qemu-system-i386', '-cdrom', ISO, '-boot', 'd', '-vga', 'std',
             '-m', '256', '-no-reboot', '-vnc', '127.0.0.1:9',
             '-serial', 'file:' + SERIAL,
             '-monitor', 'unix:' + os.path.join(B, 'mon.sock') + ',server,nowait'],
            cwd=B,   # HMP pmemsave 的文件名参数不能带 /（被表达式解析器吞掉），用相对路径
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        for _ in range(100):
            if os.path.exists(os.path.join(B, 'mon.sock')):
                break
            time.sleep(0.1)
        else:
            raise RuntimeError('monitor socket 未出现')
        self.mon = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.mon.connect(os.path.join(B, 'mon.sock'))
        time.sleep(0.3)
        self.mon.recv(65536)                      # 吃掉 banner
        self.spos = 0                             # serial 消费位置

    def cmd(self, c):
        self.mon.sendall((c + '\n').encode())
        time.sleep(0.05)
        try:
            self.mon.settimeout(0.2)
            self.mon.recv(65536)
        except OSError:
            pass

    def key(self, *ks, gap=0.15):
        for k in ks:
            self.cmd('sendkey ' + k)
            time.sleep(gap)

    def wait_mark(self, mark, timeout=30):
        """整体读串口文件、找标记、推进消费位置"""
        deadline = time.time() + timeout
        while time.time() < deadline:
            with open(SERIAL, 'rb') as f:
                f.seek(self.spos)
                chunk = f.read().decode('utf-8', 'replace')
            if mark in chunk:
                self.spos += chunk.index(mark) + len(mark)
                return
            time.sleep(0.1)
        raise RuntimeError(f'串口 {timeout}s 未现标记: {mark}')

    def screendump(self, name):
        p = os.path.join(B, name)
        if os.path.exists(p):
            os.remove(p)
        self.cmd('screendump ' + p)
        for _ in range(30):
            if os.path.exists(p) and os.path.getsize(p) > 54:
                break
            time.sleep(0.1)
        assert os.path.exists(p), f'screendump 未产出 {name}'
        return p

    def pmemsave_fb(self, name='fb1.bin'):
        p = os.path.join(B, name)
        self.cmd(f'pmemsave 0xfd000000 0x12c000 {name}')   # 相对 cwd(=B)
        for _ in range(30):
            if os.path.exists(p) and os.path.getsize(p) >= 0x12c000:
                break
            time.sleep(0.1)
        assert os.path.exists(p) and os.path.getsize(p) >= 0x12c000, 'pmemsave 未产出'
        return p

    def quit(self):
        try:
            self.cmd('quit')
        except OSError:
            pass
        time.sleep(0.3)
        if self.qemu.poll() is None:
            self.qemu.kill()

def load_ppm(path):
    data = open(path, 'rb').read()
    pos, lines = 0, []
    while len(lines) < 3:
        nl = data.index(b'\n', pos)
        line = data[pos:nl]
        pos = nl + 1
        if line.startswith(b'#') or not line.strip():
            continue
        lines.append(line)
    assert lines[0] == b'P6', 'not P6 ppm'
    w, h = map(int, lines[1].split())
    return w, h, data[pos:]

def px(img, x, y):
    w, h, raw = img
    o = (y * w + x) * 3
    return (raw[o], raw[o+1], raw[o+2])

def near(a, b, tol=14):
    return all(abs(x - y) <= tol for x, y in zip(a, b))

OK = True
def check(name, cond, detail=''):
    global OK
    if cond:
        say(f'[ok] {name}')
    else:
        say(f'[FAIL] {name} {detail}')
        OK = False

def box_has_dark(img, x0, y0, x1, y1, thr=110):
    """区域内存在暗像素（有字）"""
    for y in range(y0, y1):
        for x in range(x0, x1):
            p = px(img, x, y)
            if (p[0] + p[1] + p[2]) / 3 < thr:
                return True
    return False

def box_all_near(img, x0, y0, x1, y1, rgb, tol=14):
    """区域内全部接近某色（干净无字）"""
    for y in range(y0, y1):
        for x in range(x0, x1):
            if not near(px(img, x, y), rgb, tol):
                return False
    return True

def main():
    g = Guest()
    ok_boot = True
    try:
        say('== 阶段1: 引导（串口同步） ==')
        g.wait_mark('kernel core ready')
        g.wait_mark('[sh] shell ready')
        say('== 阶段2: 键入 gfx 启动桌面 ==')
        g.key('g', 'f', 'x', 'ret')
        g.wait_mark('S4: LFB 640x480x32')
        time.sleep(2.0)                        # 等 gfx 任务首绘 + 光标

        say('== 阶段3: screendump 验真实扫描输出 ==')
        img1 = load_ppm(g.screendump('scan1.ppm'))
        check('scan1 尺寸=640x480 (模式切换真实生效)', img1[0] == 640 and img1[1] == 480,
              f'got={img1[0]}x{img1[1]}')
        check('desktop bg', near(px(img1, 600, 100), C_DESKTOP), str(px(img1, 600, 100)))
        check('taskbar', near(px(img1, 320, 478), C_TASKBAR), str(px(img1, 320, 478)))
        check('start btn', near(px(img1, 8, 458), C_START), str(px(img1, 8, 458)))
        check('win0 标题栏(聚焦)', near(px(img1, 300, 30), C_TITLE_FOC), str(px(img1, 300, 30)))
        check('win0 背景', near(px(img1, 300, 200), C_WIN_BG), str(px(img1, 300, 200)))
        check('win0 窗内有字(shell提示符)', box_has_dark(img1, 34, 44, 340, 53))

        say('== 阶段4: pmemsave 验显存 ==')
        fb = g.pmemsave_fb('fb1.bin')
        r = subprocess.run(['python3', os.path.join(STAGE, '..', '..', 'scripts', 'check-fb.py'), fb],
                           capture_output=True, text=True)
        say(r.stdout.strip())
        check('check-fb 显存断言', r.returncode == 0)

        say('== 阶段5: ESC 新开 shell 窗 + 焦点转移 ==')
        g.key('esc', gap=0.1)
        time.sleep(1.5)
        img2 = load_ppm(g.screendump('scan2.ppm'))
        check('win1 标题栏(新窗获得聚焦)', near(px(img2, 400, 30), C_TITLE_FOC), str(px(img2, 400, 30)))
        check('win0 标题栏外露条带(失焦变灰)', near(px(img2, 70, 30), C_TITLE_UNFOC), str(px(img2, 70, 30)))
        check('win1 窗内有新 shell 输出', box_has_dark(img2, 124, 44, 460, 73))

        say('== 阶段6: 聚焦窗键入 + 回车提交（回归 \\n 修复与 stdin 路由） ==')
        # 行几何：新 shell 首条 uprint("\n...") flush 出空行 → 行号整体 +1：
        # y44 空 | y54 banner | y64 "sh> hi"(输入行) | 回车后 y74 报错、y84 新输入行
        g.key('h', 'i')
        time.sleep(0.6)
        img3 = load_ppm(g.screendump('scan3.ppm'))
        check('输入行出现键入字符', box_has_dark(img3, 156, 64, 460, 72))
        g.key('ret')
        time.sleep(1.0)
        img4 = load_ppm(g.screendump('scan4.ppm'))
        check('回车后整行作为内容行保留', box_has_dark(img4, 124, 64, 460, 72))
        check('输入行移到下一行且无残留', box_all_near(img4, 164, 84, 460, 93, C_WIN_BG),
              '新输入行位置仍有残留像素')
        check('shell 对未知命令的报错已进窗', box_has_dark(img4, 124, 74, 460, 82))

        say('== 阶段7: 鼠标点击切焦点（点 win0 外露条带） ==')
        g.cmd('mouse_move -220 -208')         # (320,240) -> (100,32)
        time.sleep(0.4)
        g.cmd('mouse_button 1')
        time.sleep(0.2)
        g.cmd('mouse_button 0')
        time.sleep(0.8)
        img5 = load_ppm(g.screendump('scan5.ppm'))
        check('点击后 win0 标题栏重新聚焦', near(px(img5, 40, 30), C_TITLE_FOC), str(px(img5, 40, 30)))
        check('点击后 win1 失焦', near(px(img5, 400, 30), C_TITLE_UNFOC), str(px(img5, 400, 30)))

        say('== 阶段8: shell 里启动 GUI 应用 calc ==')
        g.key('c', 'a', 'l', 'c', 'ret')
        time.sleep(2.0)
        img6 = load_ppm(g.screendump('scan6.ppm'))
        check('第三窗(calc)出现并聚焦', near(px(img6, 300, 90), C_TITLE_FOC), str(px(img6, 300, 90)))

        say('== 阶段9: 串口卫生（探针不得回流） ==')
        ser = open(SERIAL, 'rb').read().decode('utf-8', 'replace')
        for junk in ('[kf]', '[m b=', '[m]', '[click]', '[100x100', '[dd0', '[exec fail]'):
            check(f'串口无探针 {junk}', junk not in ser)
    except Exception as e:
        ok_boot = False
        say(f'[FAIL] 异常: {e}')
    finally:
        g.quit()
    if not ok_boot or not OK:
        say('[FAIL] s4-test')
        sys.exit(1)
    say('[PASS] s4-test')

main()
