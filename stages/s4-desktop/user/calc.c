/* calc —— 图形计算器：canvas 画布 + 按钮网格 + 事件循环
 * 显示区在顶，按钮 4x5；点击/键盘双路输入，递归下降求值（+ - * / 括号）。
 * 事件模型见 win.c 顶部注释：应用压绘制命令、收键鼠事件、wyield 让出。
 */
#include "../user/ulib.h"

/* ---------- 求值器（递归下降，整数运算） ---------- */
static const char *expr;
static int err;
static long eval_e(void);
static void ws(void) { while (*expr == ' ') expr++; }
static long eval_n(void) {
    ws();
    if (*expr == '(') {
        expr++;
        long v = eval_e();
        ws();
        if (*expr == ')') expr++; else err = 1;
        return v;
    }
    if (*expr == '-') { expr++; return -eval_n(); }
    long v = 0;
    if (*expr < '0' || *expr > '9') { err = 1; return 0; }
    while (*expr >= '0' && *expr <= '9') { v = v * 10 + (*expr - '0'); expr++; }
    return v;
}
static long eval_m(void) {
    long v = eval_n();
    for (;;) {
        ws();
        if (*expr == '*') { expr++; v *= eval_n(); }
        else if (*expr == '/') {
            expr++; ws();
            long d = eval_n();
            if (!d) { err = 2; return 0; }
            v /= d;
        } else return v;
    }
}
static long eval_e(void) {
    long v = eval_m();
    for (;;) {
        ws();
        if (*expr == '+') { expr++; v += eval_m(); }
        else if (*expr == '-') { expr++; v -= eval_m(); }
        else return v;
    }
}

/* ---------- UI（按 winsize 自适应） ---------- */
static long BW, GH;
static int GY;
#define BX(c) (8 + (c) * (int)(BW + 6))
#define BY(r) (GY + (r) * (int)(GH + 6))
static const char labels[20] = { 'C','(',')','/', '7','8','9','*', '4','5','6','-',
                                 '1','2','3','+', '0','<','=','q' };
static char eb[64];  static int el;      /* 表达式 */
static char rb[40];  static int rl;      /* 结果行 "= N" / "error" */

static void put_res(const char *s) {
    for (rl = 0; s[rl] && rl < 38; rl++) rb[rl] = s[rl];
    rb[rl] = 0;
}
static void draw_display(void) {
    struct gfxrect r = { 8, 8, 384, 44, 0x1B1B1B };
    struct gfxtext t  = { 14, 12, 0xFFFFFF, {0} };
    struct gfxtext t2 = { 14, 30, 0x2ECC71, {0} };
    int i;
    wrect(&r);
    for (i = 0; i < 45 && i < el; i++) t.s[i] = eb[i];
    wtext(&t);
    for (i = 0; i < 45 && i < rl; i++) t2.s[i] = rb[i];
    wtext(&t2);
}
static void draw_buttons(void) {
    for (int k = 0; k < 20; k++) {
        struct gfxrect r = { BX(k % 4), BY(k / 4), BW, GH - 6,
                             (labels[k] >= '0' && labels[k] <= '9') ? 0x2E86DEu :
                             (labels[k] == '=') ? 0x27AE60u :
                             (labels[k] == 'q') ? 0xC0392Bu : 0x57606Fu };
        struct gfxtext t = { BX(k % 4) + BW / 2 - 4, BY(k / 4) + (GH - 6) / 2 - 4, 0xFFFFFFu, {0} };
        t.s[0] = labels[k];
        wrect(&r);
        wtext(&t);
    }
}
static void evaluate(void) {
    if (!el) return;
    char tmp[64];
    int i;
    for (i = 0; i < el; i++) tmp[i] = eb[i];
    tmp[i] = 0;
    expr = tmp; err = 0;
    long v = eval_e();
    if (err) { put_res(err == 2 ? "divide by zero" : "error"); return; }
    rb[0] = '='; rb[1] = ' '; rl = 2;
    char d[12]; int j = 11; d[j] = 0;
    long v2 = v < 0 ? -v : v;
    do { d[--j] = (char)('0' + v2 % 10); v2 /= 10; } while (v2);
    if (v < 0 && rl < 38) rb[rl++] = '-';
    while (d[j] && rl < 38) rb[rl++] = d[j++];
    rb[rl] = 0;
}
static void action(char k) {
    if (k == 'C') { el = 0; eb[0] = 0; rl = 0; rb[0] = 0; }
    else if (k == '<') { if (el) eb[--el] = 0; }
    else if (k == '=') evaluate();
    else if (k == 'q') sys(3, 0, 0, 0);          /* exit：窗口随任务关闭 */
    else if (el < 60) {
        const char *ok = "0123456789+-*/() ";
        int good = 0;
        for (int i = 0; ok[i]; i++) if (ok[i] == k) good = 1;
        if (good) { eb[el++] = (char)k; eb[el] = 0; }
    }
    draw_display();
}

void user_main(void) {
    long W, H;
    sys(11, (intptr_t)"/dev/tty", 0, 0);
    wgfx();
    winsize(&W, &H);
    BW = (W - 16 - 3 * 6) / 4;
    GH = 38;
    GY = 58;
    draw_buttons();
    draw_display();
    for (;;) {
        struct wxev e;
        if (!wevent(&e)) { wyield(); continue; }
        if (e.type == 2) {
            int col = ((int)e.a - 8) / (int)(BW + 6), row = ((int)e.b - GY) / (int)(GH + 6);
            int lx = (int)e.a - 8 - col * (int)(BW + 6), ly = (int)e.b - GY - row * (int)(GH + 6);
            if (col >= 0 && col < 4 && row >= 0 && row < 5
             && lx >= 0 && lx < (int)BW && ly >= 0 && ly < (int)GH - 6)
                action(labels[row * 4 + col]);
        } else if (e.type == 1) {
            char c = (char)e.a;
            if (c == '\n') action('=');          /* 回车=等号 */
            else if (c == 8) action('<');
            else action(c);
        }
    }
}
