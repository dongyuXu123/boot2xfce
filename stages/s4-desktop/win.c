/* win.c —— S4：窗口系统 + 多 shell + 鼠标焦点 + 应用窗口
 *
 * 窗口= {矩形, 标题, 属主 pid, 内容行环, 输入行}。
 * - shell 窗口：/dev/tty 的 read=取本窗输入行；write=内容追加行
 * - 鼠标：移动=光标；左键点击=聚焦命中的最上层窗口
 * - 键盘：进聚焦窗口的输入行（shell 由此收指令）
 * - ESC=新开 shell 窗口（spawn /bin/sh）；时钟窗每秒刷新
 * 验收：sendkey + mouse_move/mouse_button + pmemsave 像素断言
 */
#include "kernel.h"
#include "font8x8.h"

#define C_DESKTOP      0x1E3A5F
#define C_TASKBAR      0x14202E
#define C_TITLE_FOC    0x2E86DE
#define C_TITLE_UNFOC  0x57606F
#define C_BORDER_FOC   0xF9CA24
#define C_BORDER_UNFOC 0x353B48
#define C_WIN_BG       0xF4F4F4
#define C_TEXT         0x111111
#define C_START        0x4CD137

#define MAX_WIN 6
#define WIN_COLS 46
#define WIN_ROWS 18

struct win {
    int used, is_tty;
    uint32_t pid;               /* 属主任务（0=无属主的信息窗） */
    uint32_t x, y, w, h;
    char title[24];
    char lines[WIN_ROWS][WIN_COLS];
    int top;                    /* 行环起点 */
    int nl;                     /* 有效行数 */
    char input[WIN_COLS];
    int ilen;
};
static struct win wins[MAX_WIN];
static int focus = 0;
static int gfx_on = 0;
static int gfx_dirty = 1;

int gfx_active(void) { return gfx_on; }

static struct win *win_by_pid(uint32_t pid) {
    for (int i = 0; i < MAX_WIN; i++)
        if (wins[i].used && wins[i].pid == pid) return &wins[i];
    return 0;
}

/* ---------- 绘制 ---------- */
static void dchar(uint32_t x, uint32_t y, char c, uint32_t rgb) {
    const unsigned char *g = font8x8_basic[(int)c & 0x7F];
    for (int j = 0; j < 8; j++)
        for (int i = 0; i < 8; i++)
            if (g[j] & (1u << i)) fb_pixel(x + i, y + j, rgb);
}
static void dtext(uint32_t x, uint32_t y, const char *s, uint32_t rgb, int maxcols) {
    for (int k = 0; *s && k < maxcols; s++, k++, x += 8) dchar(x, y, *s, rgb);
}
static void draw_window(int i) {
    struct win *w = &wins[i];
    int foc = (i == focus);
    fb_rect(w->x, w->y, w->w, w->h, C_WIN_BG);
    fb_rect(w->x, w->y, w->w, 16, foc ? C_TITLE_FOC : C_TITLE_UNFOC);
    dtext(w->x + 3, w->y + 4, w->title, 0xFFFFFF, (w->w - 6) / 8);
    /* 内容：行环 + 当前输入行 */
    int cols = (w->w - 8) / 8;
    int rows = (w->h - 22) / 10;
    if (rows < 1) rows = 1;
    for (int r = 0; r < rows - 1 && r < w->nl; r++) {
        int idx = (w->top + r) % WIN_ROWS;
        dtext(w->x + 4, w->y + 20 + r * 10, w->lines[idx], C_TEXT, cols);
    }
    if (w->is_tty) {
        int r = w->nl < rows - 1 ? w->nl : rows - 2;
        dtext(w->x + 4, w->y + 20 + r * 10, w->input, C_TEXT, cols);
        if (foc) fb_rect(w->x + 4 + w->ilen * 8, w->y + 20 + r * 10, 6, 8, C_TEXT);
    }
    uint32_t bc = foc ? C_BORDER_FOC : C_BORDER_UNFOC;
    for (int t = 0; t < 2; t++) {
        fb_rect(w->x - t, w->y - t, w->w + 2 + 2 * t, 1, bc);
        fb_rect(w->x - t, w->y + w->h + t, w->w + 2 + 2 * t, 1, bc);
        fb_rect(w->x - t, w->y - t, 1, w->h + 2 + 2 * t, bc);
        fb_rect(w->x + w->w + t, w->y - t, 1, w->h + 2 + 2 * t, bc);
    }
}
static void draw_desktop(void) {
    print("[dd0 t=");
    { extern volatile uint32_t ticks; print_dec(ticks); print("]");
      uint32_t t0 = ticks;
    fb_rect(0, 0, 100, 100, C_DESKTOP);            /* 小块试填充 */
    print("[100x100 t=");
    { extern volatile uint32_t ticks; print_dec(ticks); }
    print(" t0="); print_dec(t0); print("]"); }
    { extern volatile uint32_t ticks; uint32_t q = ticks;
      fb_rect(0, 0, fb_w, fb_h, C_DESKTOP);
      print("[full t0="); print_dec(q); print(" t1="); print_dec(ticks); print("]\n"); }
    fb_rect(0, fb_h - 28, fb_w, 28, C_TASKBAR);
    fb_rect(6, fb_h - 24, 90, 18, C_START);
    fb_text(14, fb_h - 20, "boot2xfce", 0x111111);
    fb_text(fb_w - 170, fb_h - 20, "ESC:new  click:focus", 0x8395A7);
    for (int i = 0; i < MAX_WIN; i++)
        if (wins[i].used) draw_window(i);
}

/* ---------- 光标（保存区 + 箭头精灵） ---------- */
static uint32_t cur_bg[12 * 18];
static int cur_x = 320, cur_y = 240, cur_on = 0;
static const char arrow[12] = { 0x40,0x60,0x70,0x78,0x7C,0x7E,0x7F,0x7C,0x5C,0x1C,0x0E,0x06 };
static void cursor_draw(void) {
    for (int j = 0; j < 12; j++)
        for (int i = 0; i < 8; i++) {
            uint32_t c = (arrow[j] & (0x80u >> i)) ? 0xFFFFFF : 0x000000;
            cur_bg[j * 8 + i] = ((volatile uint32_t *)(uintptr_t)(fb_addr + (cur_y + j) * fb_pitch))[cur_x + i] = c;
        }
    cur_on = 1;
}
static void cursor_erase(void) {
    for (int j = 0; j < 12; j++)
        for (int i = 0; i < 8; i++)
            ((volatile uint32_t *)(uintptr_t)(fb_addr + (cur_y + j) * fb_pitch))[cur_x + i] = cur_bg[j * 8 + i];
    cur_on = 0;
}

/* ---------- 窗口操作 ---------- */
static int win_new(const char *title, uint32_t pid, int is_tty) {
    /* 同属主已有窗口则直接复用（一个任务一个窗） */
    if (pid) {
        struct win *ex = win_by_pid(pid);
        if (ex) { focus = (int)(ex - wins); return (int)(ex - wins); }
    }
    for (int i = 0; i < MAX_WIN; i++) {
        if (wins[i].used) continue;
        memset(&wins[i], 0, sizeof(wins[i]));
        wins[i].used = 1;
        wins[i].pid = pid;
        wins[i].is_tty = is_tty;
        for (int j = 0; j < 23 && title[j]; j++) wins[i].title[j] = title[j];
        wins[i].x = 30 + (i % 2) * 90;
        wins[i].y = 24 + (i / 2) * 60;
        wins[i].w = 560;
        wins[i].h = 400;
        focus = i;
        print("[win] "); print(title); print("\n");
        return i;
    }
    return -1;
}
static void win_puts(int i, const char *s) {         /* 追加内容行（\n 分段） */
    struct win *w = &wins[i];
    for (; *s; s++) {
        if (*s == '\n' || w->ilen >= WIN_COLS - 1) {
            w->lines[(w->top + w->nl) % WIN_ROWS][0] = 0;
            memcpy(w->lines[(w->top + w->nl) % WIN_ROWS], w->input, WIN_COLS);
            if (w->nl < WIN_ROWS) w->nl++; else w->top = (w->top + 1) % WIN_ROWS;
            memset(w->input, 0, WIN_COLS);
            w->ilen = 0;
            if (*s != '\\n') w->input[w->ilen++] = *s;
        } else w->input[w->ilen++] = *s;
    }
}

/* ---------- 应用/任务联动（sys 22 spawn / 窗口跟随） ---------- */
int gfx_spawn_window(uint32_t pid, const char *title) {
    int i = win_new(title, pid, 1);
    draw_desktop();
    return i;
}
void gfx_on_exit(uint32_t pid) {                     /* 任务退出关窗 */
    for (int i = 0; i < MAX_WIN; i++)
        if (wins[i].used && wins[i].pid == pid) { wins[i].used = 0; }
}

/* ---------- tty 联动（tty.c 调用） ---------- */
static struct wtty { int used, ready, len; char line[120]; } wt[MAX_WIN];
static struct wtty legacy;                           /* gfx 前的全局输入 */
static int legacy_on = 1;

void tty_feed_global(char c) {                       /* gfx 前的键入 */
    if (c == '\\n' || (legacy.len && legacy.len < 119)) {
        if (c != '\\n') legacy.line[legacy.len++] = c;
        legacy.ready = 1;
    }
}
int tty_read_global(char *out, int max) {
    while (!legacy.ready) schedule();
    int n = legacy.len < max ? legacy.len : max - 1;
    memcpy(out, legacy.line, n); out[n] = 0;
    legacy.len = 0; legacy.ready = 0;
    return n;
}
int win_by_pid_ok(uint32_t pid) {
    return win_by_pid(pid) != 0;
}
void gfx_console_write(uint32_t pid, const char *s, uint32_t n) {
    struct win *w = win_by_pid(pid);
    if (!w) return;
    char tmp[256];
    uint32_t k = n < 255 ? n : 255;
    memcpy(tmp, s, k); tmp[k] = 0;
    win_puts((int)(w - wins), tmp);
    gfx_dirty = 1;
}

void tty_write_window(uint32_t pid, const char *s) { /* shell 输出进窗 */
    struct win *w = win_by_pid(pid);
    if (w) win_puts((int)(w - wins), s);
}
/* gfx 模式键盘：喂聚焦窗口输入行；行就绪置 ready */
static struct wtty *focus_wt(void) {
    struct win *w = &wins[focus];
    if (!w->used || !w->is_tty) return 0;
    return &wt[focus];
}
void gfx_key_feed(char c) {
    print("[kf]");
    struct wtty *t = focus_wt();
    if (!t) { print("[no-wt]"); return; }
    if (c == '\\b') { if (t->len) { t->len--; struct win *w = &wins[focus]; if (w->ilen) { w->ilen--; w->input[w->ilen] = 0; } } return; }
    if (c == '\\n') { t->ready = 1; return; }
    if (t->len < 118) {
        t->line[t->len++] = c;
        struct win *w = &wins[focus];
        if (w->ilen < WIN_COLS - 1) { w->input[w->ilen++] = c; w->input[w->ilen] = 0; }
    }
}
/* shell 的 tty read：等自己窗口的行 */
int tty_read_window(uint32_t pid, char *out, int max) {
    struct win *w = win_by_pid(pid);
    if (!w) return tty_read_global(out, max);
    struct wtty *t = &wt[(int)(w - wins)];
    for (;;) {
        if (t->ready) {
            int n = t->len < max ? t->len : max - 1;
            memcpy(out, t->line, n); out[n] = 0;
            t->len = 0; t->ready = 0;
            /* 输入行进内容区 */
            win_puts((int)(w - wins), "\\n");
            return n;
        }
        schedule();
    }
}

/* ---------- 启动/事件 ---------- */
void gfx_start(void) {
    gfx_on = 1;
    legacy_on = 0;
    /* 给现有 shell 任务挂窗口（键盘与输出随之进窗） */
    extern task_t *current;
    if (win_new("shell", current->pid, 1) >= 0) { }
}

static int spawn_shell(void) {
    extern int task_spawn(const char *path, uint32_t ppid, const char *title);
    return task_spawn("/bin/sh", 0, "shell");
}

void gfx_task(void);
void gfx_task(void) {
    static int fb_ready = 0;
    static uint32_t lt = 0;
    uint8_t btn; int16_t dx, dy;
    for (;;) {
        if (!gfx_active()) { task_yield(); continue; }
        if (!fb_ready) {
            fb_init();
            draw_desktop();
            cursor_draw();
            fb_ready = 1;
            task_yield(); continue;
        }
        /* 鼠标事件 */
        while (mouse_pop(&btn, &dx, &dy)) {
            { static int d = 0; if (d++ < 30) { print("[m b="); print_dec(btn);
              print(" to="); print_dec((uint32_t)(cur_x + dx)); print(","); print_dec((uint32_t)(cur_y - dy));
              print("]"); } }
            { static int d = 0; if (d++ < 40) { print("[m]"); print_dec((uint32_t)btn); print(","); print_dec((uint32_t)(dx < 0 ? -dx : dx)); print(","); print_dec((uint32_t)(dy < 0 ? -dy : dy)); print("]"); } }
            cursor_erase();
            cur_x += dx; cur_y -= dy;
            if (cur_x < 0) cur_x = 0; if (cur_x > (int)fb_w - 9) cur_x = (int)fb_w - 9;
            if (cur_y < 0) cur_y = 0; if (cur_y > (int)fb_h - 13) cur_y = (int)fb_h - 13;
            if (btn & 1) {
                print("[click at "); print_dec((uint32_t)cur_x); print(","); print_dec((uint32_t)cur_y);
                print(" -> focus "); print_dec((uint32_t)focus); print("]");
                print("[click]");
                for (int i = MAX_WIN - 1; i >= 0; i--) {
                    struct win *w = &wins[i];
                    if (!w->used) continue;
                    if (cur_x >= (int)w->x && cur_x < (int)(w->x + w->w)
                     && cur_y >= (int)w->y && cur_y < (int)(w->y + w->h)) {
                        focus = i;
                        break;
                    }
                }
                draw_desktop();
            }
            cursor_draw();
        }
        /* 键盘 */
        int c;
        while ((c = kbd_pop()) >= 0) {
            if (c == 27) {                       /* ESC：新 shell 窗口 */
                int pid = spawn_shell();
                if (pid) { gfx_spawn_window((uint32_t)pid, "shell"); draw_desktop(); }
                continue;
            }
            if (c == '\\n' || (c >= 32 && c < 127) || c == 8) gfx_key_feed((char)c);
            draw_desktop();
        }
        task_yield();
    }
}
