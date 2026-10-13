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
        if (streq(line, "help")) {
            uprint("commands: run hello | echo <s> | gfx | help\n");
        } else if (streq(line, "gfx")) {
            uprint("entering gfx (keyboard goes to GUI)\n");
            sys(17, 0, 0, 0);
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
