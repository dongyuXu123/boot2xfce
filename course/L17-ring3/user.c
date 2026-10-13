/* L17 桩 —— ring3 用户程序（int 0x80 是你与世界唯一的接口）
 * _start→user_main→uprint 链路给定；你写 sys() 这一个内联汇编函数。
 * 验收：串口出现 "[user] hello from ring3, pid=N"。
 * 大坑提醒：内联汇编必须带 "memory" clobber——想想内核要从用户栈
 * 拷数据，如果 GCC 把栈缓冲的存储重排到 int 指令之后会怎样。 */
typedef unsigned int uint32_t;
typedef long intptr_t;

static long sys(long n, long a, long b, long c) {
    /* TODO(你来写·L17):
     * __asm__ volatile("int $0x80" : "=a"(r)
     *                  : "a"(n), "b"(a), "c"(b), "d"(c)
     *                  : "memory");
     * 各约束都是什么意思？为什么 r 用 "=a"？ */
    long r = 0;
    (void)n; (void)a; (void)b; (void)c;
    return r;
}
static void uprint(const char *s) {
    long n = 0;
    while (s[n]) n++;
    sys(1, 1, (intptr_t)s, n);
}
static void uprint_dec(long v) {
    char b[12];
    int i = 11;
    b[i] = 0;
    do { b[--i] = (char)('0' + v % 10); v /= 10; } while (v);
    sys(1, 1, (intptr_t)&b[i], 11 - i);
}

void user_main(void) {
    uprint("[user] hello from ring3, pid=");
    uprint_dec(sys(2, 0, 0, 0));
    uprint("\n");
    uprint("[user] uptime=");
    uprint_dec(sys(4, 0, 0, 0));
    uprint(" ticks\n");
    sys(5, 0, 0, 0);                    /* ls：内核侧打印 ramfs 文件表 */
    sys(3, 0, 0, 0);                    /* exit */
    for (;;) ;                          /* DEAD 后不会再被调度，保险空转 */
}
