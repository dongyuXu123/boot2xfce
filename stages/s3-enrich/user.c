/* user.c —— ring3 演示程序（独立编译成平铺二进制，内核拷到 USER_BASE）
 * 唯一能用的 API：int 0x80。链接到 0x400000，_start 就是入口。 */
typedef unsigned int uint32_t;
typedef long intptr_t;

static long sys(long n, long a, long b, long c) {
    long r;
    /* "memory" clobber：告诉 GCC 别把栈上缓冲的存储重排到 int 0x80 之后
     * （否则内核从用户栈拷贝时读到的还是旧内容） */
    __asm__ volatile("int $0x80"
                     : "=a"(r)
                     : "a"(n), "b"(a), "c"(b), "d"(c)
                     : "memory");
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
    /* ---- fork/COW/wait 演示：子改父不改，COW 才算成立 ---- */
    int x = 5;                           /* 用户数据/栈页：fork 时转 COW 共享 */
    long pid = sys(7, 0, 0, 0);          /* fork */
    if (pid == 0) {
        x = 6;                           /* 写 → COW 缺页 → 私有副本 */
        uprint("[child] fork=0, x=");
        uprint_dec(x);
        uprint("\n");
        sys(3, 0, 0, 0);                 /* exit */
    } else {
        long w = sys(8, 0, 0, 0);        /* wait 收尸 */
        uprint("[parent] child ");
        uprint_dec(w);
        uprint(" reaped, x=");
        uprint_dec(x);
        uprint("\n");
        sys(3, 0, 0, 0);
    }
    sys(5, 0, 0, 0);                    /* ls：内核侧打印 ramfs 文件表 */
    for (;;) ;                          /* DEAD 后不会再被调度，保险空转 */
}
