/* notepad —— 图形记事本：canvas 编辑区 + 底部按钮（Save/Clear/Quit）
 * 键入直达编辑区（回车提交行、退格改行），Save 固定写 /home/note.txt。
 * 整帧重绘 = 清底 + 内容行 + 当前行 + 光标 + 按钮，命令环一次flush。
 */
#include "../user/ulib.h"

#define LINEMAX 66

static char buf[24][LINEMAX];
static int bn;
static char cur[LINEMAX];
static int cl;                       /* 当前行长 */
static const char *status = "notepad - type, Enter for new line";
static long BH;                      /* 画布高（winsize 取回） */

static void btn(int x, const char *label, unsigned color) {
    struct gfxrect r = { x, BH - 34, 64, 22, color };
    struct gfxtext t = { x + 8, BH - 27, 0xFFFFFFu, {0} };
    int i;
    wrect(&r);
    for (i = 0; label[i]; i++) t.s[i] = label[i];
    wtext(&t);
}
static void repaint(void) {
    struct gfxrect bg = { 0, 0, 4096, 4096, 0xFAFAFAu };
    struct gfxtext st = { 10, 8, 0x7F8C8Du, {0} };
    int i;
    wrect(&bg);
    for (i = 0; status[i] && i < 46; i++) st.s[i] = status[i];
    wtext(&st);
    for (i = 0; i < bn; i++) {       /* 已提交行 */
        struct gfxtext t = { 8, 20 + i * 9, 0x222222u, {0} };
        int k;
        for (k = 0; buf[i][k] && k < 44; k++) t.s[k] = buf[i][k];
        wtext(&t);
    }
    {                                 /* 当前行 + 方块光标 */
        struct gfxtext t = { 8, 20 + bn * 9, 0x222222u, {0} };
        struct gfxrect csr = { 8 + cl * 8, 20 + bn * 9, 6, 8, 0x222222u };
        for (i = 0; cur[i] && i < 44; i++) t.s[i] = cur[i];
        wtext(&t);
        wrect(&csr);
    }
    btn(8,   "Save",  0x27AE60u);
    btn(80,  "Clear", 0x57606Fu);
    btn(152, "Quit",  0xC0392Bu);
}
static void save(void) {
    int fd = sys(11, (intptr_t)"/home/note.txt", 0, 0);
    if (fd < 0) { status = "save failed"; return; }
    for (int i = 0; i < bn; i++) {
        int n2 = 0;
        while (buf[i][n2]) n2++;
        if (n2) sys(13, fd, (intptr_t)buf[i], n2);
        sys(13, fd, (intptr_t)"\n", 1);
    }
    sys(14, fd, 0, 0);
    status = "saved to /home/note.txt";
}
static void click(long x, long y) {
    if (y >= BH - 34 && y < BH - 12) {
        if (x >= 8 && x < 72) save();
        else if (x >= 80 && x < 144) { bn = 0; cl = 0; cur[0] = 0; status = "cleared"; }
        else if (x >= 152 && x < 216) sys(3, 0, 0, 0);   /* exit 关窗 */
    }
}

void user_main(void) {
    sys(11, (intptr_t)"/dev/tty", 0, 0);
    wgfx();
    winsize(&BH, &BH);                   /* 只用高度 */
    repaint();
    for (;;) {
        struct wxev e;
        if (!wevent(&e)) { wyield(); continue; }
        if (e.type == 2) click(e.a, e.b);
        else if (e.type == 1) {
            char c = (char)e.a;
            if (c == '\n') {                             /* 提交当前行 */
                if (bn < 24) {
                    int i;
                    for (i = 0; i < cl; i++) buf[bn][i] = cur[i];
                    buf[bn][i] = 0;
                    bn++;
                }
                cl = 0; cur[0] = 0;
            } else if (c == 8) {
                if (cl) cur[--cl] = 0;
            } else if (c >= 32 && c < 127 && cl < LINEMAX - 1) {
                cur[cl++] = c;
                cur[cl] = 0;
            }
        }
        repaint();
    }
}
