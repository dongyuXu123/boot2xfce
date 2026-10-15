/* clock —— 图形时钟：canvas 画模拟表盘（时针/分针/秒针）+ 数字时间
 * 无浮点：毫弧正弦表 6° 一档（0..90°，x1000），镜像/对称展开到全圆。
 * RTC sys23 给 HHMMSS，秒变即整帧重绘（清底→表圈→刻度→针→数字）。
 */
#include "../user/ulib.h"

static long CX, CY, R;

static const short SIN6[16] = {            /* sin(6k°)*1000, k=0..15 */
    0, 105, 208, 309, 407, 500, 588, 669,
    743, 809, 866, 914, 951, 978, 995, 1000
};
static int isin(int deg) {                 /* 任意整数角度 → x1000 */
    int sign = 1;
    deg %= 360;
    if (deg < 0) deg += 360;
    if (deg > 180) { sign = -1; deg -= 180; }
    if (deg > 90) deg = 180 - deg;
    return sign * SIN6[deg / 6];
}
static int icos(int deg) { return isin(deg + 90); }

static void hand(long deg, long len, unsigned color) {
    struct gfxrect l = { CX, CY,
                         CX + len * icos(deg) / 1000,
                         CY - len * isin(deg) / 1000, color };
    wrect(&l);                             /* LINE 语义：w,h 复用为终点 */
}
static void draw_digit(const char *s) {
    struct gfxtext t = { CX - 32, CY + R + 22, 0x8395A7, {0} };
    int i;
    for (i = 0; s[i] && i < 40; i++) t.s[i] = s[i];
    wtext(&t);
}
static void repaint(uint32_t t) {
    long h = t / 10000, m = t / 100 % 100, s = t % 100;
    struct gfxrect bg = { 0, 0, 4096, 4096, 0x0F1420u };
    struct gfxrect hub = { CX - 3, CY - 3, 6, 6, 0xE8E8E8u };
    wrect(&bg);
    wcircle(CX, CY, R, 0x8395A7u);         /* 表圈 */
    for (int k = 0; k < 60; k++) {         /* 刻度：5 的倍数长而亮 */
        long r2 = (k % 5 == 0) ? R * 88 / 100 : R * 95 / 100;
        wline(CX + R * icos(k * 6) / 1000,
              CY - R * isin(k * 6) / 1000,
              CX + r2 * icos(k * 6) / 1000,
              CY - r2 * isin(k * 6) / 1000,
              (k % 5 == 0) ? 0xE8E8E8u : 0x57606Fu);
    }
    hand((h % 12) * 30 + m / 2, R * 47 / 100, 0xE8E8E8u);
    hand(m * 6, R * 71 / 100, 0xE8E8E8u);
    hand(s * 6, R * 91 / 100, 0xE74C3Cu);
    wrect(&hub);
    char d[9];
    d[0] = (char)('0' + h / 10); d[1] = (char)('0' + h % 10);
    d[2] = ':';
    d[3] = (char)('0' + m / 10); d[4] = (char)('0' + m % 10);
    d[5] = ':';
    d[6] = (char)('0' + s / 10); d[7] = (char)('0' + s % 10);
    d[8] = 0;
    draw_digit(d);
}

void user_main(void) {
    long w = 0, h = 0;
    sys(11, (intptr_t)"/dev/tty", 0, 0);
    wgfx();
    winsize(&w, &h);
    CX = w / 2;
    CY = (h - 60) / 2 + 10;
    R = (w < h ? w : h) / 2 - 30;
    if (R < 40) R = 40;
    uint32_t last = 0xFFFFFFFFu;
    for (;;) {
        uint32_t t = sys(23, 0, 0, 0);
        if (t != last) { last = t; repaint(t); }
        wyield();
    }
}
