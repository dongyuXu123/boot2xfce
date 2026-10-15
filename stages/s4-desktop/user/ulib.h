/* ulib.h —— 用户程序共享的小运行时（唯一 API：int 0x80） */
#ifndef ULIB_H
#define ULIB_H
typedef unsigned int uint32_t;
typedef long intptr_t;

static long sys(long n, long a, long b, long c) {
    long r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "b"(a), "c"(b), "d"(c) : "memory");
    return r;
}
static void uprint(const char *s) {
    long n = 0; while (s[n]) n++;
    sys(1, 1, (intptr_t)s, n);
}
static void uprint_dec(long v) {
    char b[12]; int i = 11; b[i] = 0;
    do { b[--i] = (char)('0' + v % 10); v /= 10; } while (v);
    sys(1, 1, (intptr_t)&b[i], 11 - i);
}

/* ---- 图形窗口 API（S4 canvas 模式，win.c 顶部注释述其原理） ---- */
struct gfxrect { int x, y, w, h; unsigned color; };
struct gfxtext { int x, y; unsigned color; char s[48]; };
struct wxev { long type, a, b; };       /* 1=键(a=ASCII) 2=点击(a=x,b=y) */

static void wgfx(void)                     { sys(25, 0, 0, 0); }
static void wrect(const struct gfxrect *r) { sys(26, (intptr_t)r, 0, 0); }
static void wtext(const struct gfxtext *t) { sys(27, (intptr_t)t, 0, 0); }
static int  wevent(struct wxev *e)         { return (int)sys(28, (intptr_t)e, 0, 0); }
static void wyield(void)                   { sys(29, 0, 0, 0); }
static void winsize(long *w, long *h)      { long b[2] = {0, 0}; sys(30, (intptr_t)b, (intptr_t)(b + 1), 0); *w = b[0]; *h = b[1]; }
#endif
