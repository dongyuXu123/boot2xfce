/* init —— 首个用户程序（内核内嵌自启）：fork/COW/wait/execve 演示 */
#include "ulib.h"

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
        /* execve：原位换像成 /bin/hello，成功则不再返回 */
        long e = sys(10, (intptr_t)"/bin/hello", 0, 0);
        uprint("[child] execve failed ret=");
        uprint_dec(e);
        uprint("\n");
        sys(3, 0, 0, 0);
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
