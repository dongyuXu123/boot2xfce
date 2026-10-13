/* calc —— 窗口计算器：读行求值 + - * / ( )，语法分析（递归下降） */
#include "../user/ulib.h"

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

void user_main(void) {
    sys(11, (intptr_t)"/dev/tty", 0, 0);
    sys(13, 0, (intptr_t)"calc> ", 6);
    char line[80];
    for (;;) {
        long n = sys(12, 0, (intptr_t)line, 78);
        if (n <= 0) continue;
        if (line[n-1] == '\n') line[n-1] = 0; else line[n] = 0;
        if (!line[0]) { sys(13, 0, (intptr_t)"calc> ", 6); continue; }
        if (line[0] == 'q') break;
        expr = line; err = 0;
        long v = eval_e();
        if (err) sys(13, 0, (intptr_t)"calc: error\n", 12);
        else {
            char o[48]; int i = 0;
            const char *eq = "= "; while (*eq) o[i++] = *eq++;
            char d[12]; int j = 11; d[j] = 0;
            long v2 = v < 0 ? -v : v;
            do { d[--j] = (char)('0' + v2 % 10); v2 /= 10; } while (v2);
            if (v < 0) o[i++] = '-';
            while (d[j]) o[i++] = d[j++];
            o[i++] = '\n';
            sys(13, 0, (intptr_t)o, i);
        }
        sys(13, 0, (intptr_t)"calc> ", 6);
    }
}
