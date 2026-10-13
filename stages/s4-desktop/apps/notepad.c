/* notepad —— 窗口记事本：逐行输入；.l 列出 .c 清空 .s <f> 存盘 .q 退出 */
#include "../user/ulib.h"

static char buf[32][78];
static int bl[32], bn;

static void dump(void) {
    uprint("--- notepad ---\n");
    for (int i = 0; i < bn; i++) { uprint(buf[i]); uprint("\n"); }
}
void user_main(void) {
    sys(11, (intptr_t)"/dev/tty", 0, 0);
    uprint("notepad: type lines; .l list .c clear .s <file> save .q quit\n");
    char line[80];
    for (;;) {
        long n = sys(12, 0, (intptr_t)line, 78);
        if (n <= 0) continue;
        if (line[n-1] == '\n') line[n-1] = 0; else line[n] = 0;
        if (line[0] == '.' && line[1] == 'q') break;
        if (line[0] == '.' && line[1] == 'l') { dump(); continue; }
        if (line[0] == '.' && line[1] == 'c') { bn = 0; uprint("cleared\n"); continue; }
        if (line[0] == '.' && line[1] == 's') {
            int fd = sys(11, (intptr_t)line + 3, 0, 0);
            if (fd >= 0) {
                for (int i = 0; i < bn; i++) sys(13, fd, (intptr_t)buf[i], bl[i]);
                sys(14, fd, 0, 0);
                uprint("saved\n");
            } else uprint("save failed\n");
            continue;
        }
        if (bn < 32) {
            int l2 = n < 77 ? n : 77;
            memcpy(buf[bn], line, l2); buf[bn][l2] = 0; bl[bn] = l2; bn++;
        }
    }
}
