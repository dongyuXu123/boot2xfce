/* clock —— 窗口时钟：每秒把 RTC 时间打进自己的窗口 */
#include "../user/ulib.h"

void user_main(void) {
    sys(11, (intptr_t)"/dev/tty", 0, 0);
    char l2[24];
    uint32_t last = 0;
    for (;;) {
        uint32_t t = sys(23, 0, 0, 0);
        if (t != last) {
            last = t;
            l2[0] = '0' + (char)(t / 10000 % 10);
            l2[1] = '0' + (char)(t / 1000 % 10);
            l2[2] = ':';
            l2[3] = '0' + (char)(t / 100 % 10);
            l2[4] = '0' + (char)(t / 10 % 10);
            l2[5] = ':';
            l2[6] = '0' + (char)(t % 100 / 10);
            l2[7] = '0' + (char)(t % 10);
            l2[8] = 0;
            uprint(l2); uprint("\n");
        }
        for (volatile int i = 0; i < 200000; i++) ;
        sys(4, 0, 0, 0);   /* uptime 顺带让出 */
    }
}
