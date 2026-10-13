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
#endif
