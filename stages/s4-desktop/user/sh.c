/* sh —— 用户态 shell：read(0)=TTY 行规程，fork+execve 跑 /bin/hello */
#include "ulib.h"

static int streq(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

void user_main(void) {
    sys(11, (intptr_t)"/dev/tty", 0, 0);         /* fd0 = 键盘行规程 */
    uprint("\n[sh] shell ready\n");
    char line[100];
    for (;;) {
        uprint("sh> ");
        long n = sys(12, 0, (intptr_t)line, 90); /* 阻塞读一行 */
        if (n <= 0) continue;
        if (line[n - 1] == '\n') line[n - 1] = 0; else line[n] = 0;
        if (line[0] == 0) continue;
        if (streq(line, "clear")) {
            sys(24, 0, 0, 0);                    /* 清当前窗口 */
        } else if (streq(line, "help")) {
            uprint("cmds: ls [p] cd p mkdir p rm p pwd cat f run hello echo s gfx help\n");
        } else if (streq(line, "pwd")) {
            char b[128];
            sys(21, (intptr_t)b, 127, 0);
            uprint(b); uprint("\n");
        } else if (line[0]=='l' && line[1]=='s' && (line[2]==0 || line[2]==' ')) {
            sys(5, line[2] ? (intptr_t)line + 3 : 0, 0, 0);
        } else if (line[0]=='c' && line[1]=='d' && line[2]==' ') {
            if (sys(20, (intptr_t)line + 3, 0, 0) < 0) uprint("cd: bad dir\n");
        } else if (line[0]=='m' && line[1]=='k' && line[2]=='d' && line[3]=='i' && line[4]=='r' && line[5]==' ') {
            if (sys(18, (intptr_t)line + 6, 0, 0) < 0) uprint("mkdir: failed\n");
        } else if (line[0]=='r' && line[1]=='m' && line[2]==' ') {
            if (sys(19, (intptr_t)line + 3, 0, 0) < 0) uprint("rm: failed\n");
        } else if (line[0]=='c' && line[1]=='a' && line[2]=='t' && line[3]==' ') {
            int fd = sys(11, (intptr_t)line + 4, 0, 0);
            if (fd < 0) { uprint("cat: no file\n"); }
            else {
                char cb[256];
                int cn = sys(12, fd, (intptr_t)cb, 255);
                if (cn > 0) sys(1, 1, (intptr_t)cb, cn);
                uprint("\n");
                sys(14, fd, 0, 0);
            }
        } else if (streq(line, "run hello")) {
            long pid = sys(7, 0, 0, 0);          /* fork */
            if (pid == 0) {
                sys(10, (intptr_t)"/bin/hello", 0, 0);   /* execve */
                uprint("sh: exec failed\n");
                sys(3, 0, 0, 0);
            } else {
                sys(8, 0, 0, 0);                 /* wait 收尸 */
                uprint("[sh] child done\n");
            }
        } else if (streq(line, "gfx")) {
            uprint("entering gfx (keyboard goes to GUI)\n");
            sys(17, 0, 0, 0);
        } else if (streq(line, "calc") || streq(line, "clock") || streq(line, "notepad")) {
            char cmd[24] = "/bin/";
            for (int i = 0; i < 15 && line[i]; i++) cmd[5 + i] = line[i];
            long pid = sys(7, 0, 0, 0);          /* fork：GUI 应用不等待 */
            if (pid == 0) {                      /* 子：换像成应用（窗随 execve 开） */
                sys(10, (intptr_t)cmd, 0, 0);
                sys(3, 0, 0, 0);
            }
            /* 父：不 wait，立即回提示符——shell 要能继续启动其他应用 */
        } else if (line[0]=='e' && line[1]=='c' && line[2]=='h' && line[3]=='o' && line[4]==' ') {
            uprint(line + 5);
            uprint("\n");
        } else {
            uprint("sh: unknown command: ");
            uprint(line);
            uprint("\n");
        }
    }
}
