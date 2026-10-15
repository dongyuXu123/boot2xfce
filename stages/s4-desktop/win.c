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

/* ---------- 图形窗口（canvas 模式） ----------
 * 应用不直接碰帧缓冲：经 sys26/27 把绘制命令压进本窗命令环，
 * gfx_task 单线程消费并画进画布；画布 = pmm 帧 + boot_pd 别名
 * （GFX_VA 区，仅内核视角），draw_window 时整块 blit 上屏。
 * 键鼠反向走事件环：聚焦的图形窗收 EV_KEY/EV_CLICK，应用 sys28 轮询。 */
#define GFX_VA_BASE 0x06000000u      /* 每窗 1MiB 别名槽（boot_pd，supervisor） */
#define GFX_SLOT    0x100000u
#define OP_RECT     1
#define OP_TEXT     2
#define OP_LINE     3
#define OP_CIRCLE   4
struct drawop {
    int op, x, y, w, h;
    uint32_t color;
    char s[48];
};
#define RING_N 128
#define EVN 32

struct win {
    int used, is_tty;
    int is_gfx, canvas_ok;          /* canvas_ok：画布已清底、可 blit */
    uint32_t canvas;                /* 画布别名基址（0=无） */
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
static int zlist[MAX_WIN];              /* 叠放次序：zlist[0] 最底、zlist[zn-1] 最顶（存槽位号） */
static int zn;                          /* zlist 中的窗口数 */
static struct drawop ring[MAX_WIN][RING_N];
static volatile int rhead[MAX_WIN], rtail[MAX_WIN];
static struct wmev evq[MAX_WIN][EVN];
static volatile int ehead[MAX_WIN], etail[MAX_WIN];
static int focus = 0;
static int gfx_on = 0;
volatile unsigned gfx_passes, gfx_draws;   /* 诊断：gfx 调度/绘制计数 */
static volatile unsigned gfx_gen = 1;   /* 内容代数：任何改动 ++，绘制方比较代数 */

int gfx_active(void) { return gfx_on; }

static struct win *win_by_pid(uint32_t pid) {
    for (int i = 0; i < MAX_WIN; i++)
        if (wins[i].used && wins[i].pid == pid) return &wins[i];
    return 0;
}

/* ---------- 画布：分配 = pmm 帧 + boot_pd 别名（仅内核视角） ---------- */
static void canvas_free(int i) {
    struct win *w = &wins[i];
    if (!w->canvas) return;
    uint32_t va = w->canvas;
    uint32_t n = (w->w * w->h * 4 + 4095) >> 12;
    for (uint32_t k = 0; k < n; k++) {
        uint32_t pa = page_va2pa(page_kernel_pd(), va + k * 4096u);
        if (pa) { pmm_free_frame(pa); page_unmap(page_kernel_pd(), va + k * 4096u); }
    }
    w->canvas = 0; w->is_gfx = 0; w->canvas_ok = 0;
}

/* 应用侧（系统调用上下文）开图形模式：分配画布并别名进 boot_pd。
 * 清底由 gfx_task 首帧做（本上下文的 CR3 是任务的，摸不到画布）。 */
int gfx_mode_on(uint32_t pid) {
    cli();
    struct win *w = win_by_pid(pid);
    if (!w) { sti(); return -1; }
    if (!w->is_gfx) {
        int i = (int)(w - wins);
        uint32_t va = GFX_VA_BASE + (uint32_t)i * GFX_SLOT;
        uint32_t n = (w->w * w->h * 4 + 4095) >> 12, bad = 0;
        for (uint32_t k = 0; k < n && !bad; k++) {
            uint32_t pa = pmm_alloc_frame();
            if (!pa) bad = 1;
            else page_map(page_kernel_pd(), va + k * 4096u, pa, 0, 1);
        }
        if (bad) { canvas_free(i); sti(); return -1; }
        w->canvas = va;
        w->is_gfx = 1;
        w->canvas_ok = 0;
        rhead[i] = rtail[i] = 0;
        ehead[i] = etail[i] = 0;
    }
    gfx_gen++;
    sti();
    return 0;
}

/* 绘制命令：应用压入（生产者），gfx_task 消费（消费者） */
static int ring_push(int i, const struct drawop *o) {
    int ok = 0;
    cli();
    int nh = (rhead[i] + 1) % RING_N;
    if (nh != rtail[i]) { ring[i][rhead[i]] = *o; rhead[i] = nh; ok = 1; }
    gfx_gen++;
    sti();
    return ok;
}
int gfx_rect(uint32_t pid, int x, int y, int w2, int h2, uint32_t color) {
    struct win *w = win_by_pid(pid);
    if (!w || !w->is_gfx) return -1;
    struct drawop o = { OP_RECT, x, y, w2, h2, color, {0} };
    return ring_push((int)(w - wins), &o);
}
int gfx_text(uint32_t pid, int x, int y, uint32_t color, const char *s) {
    struct win *w = win_by_pid(pid);
    if (!w || !w->is_gfx) return -1;
    struct drawop o = { OP_TEXT, x, y, 0, 0, color, {0} };
    int k;
    for (k = 0; k < 47 && s[k]; k++) o.s[k] = s[k];
    o.s[k] = 0;
    return ring_push((int)(w - wins), &o);
}
int gfx_line(uint32_t pid, int x0, int y0, int x1, int y1, uint32_t color) {
    struct win *w = win_by_pid(pid);
    if (!w || !w->is_gfx) return -1;
    struct drawop o = { OP_LINE, x0, y0, x1, y1, color, {0} };
    return ring_push((int)(w - wins), &o);
}
int gfx_circle(uint32_t pid, int cx, int cy, int r, uint32_t color) {
    struct win *w = win_by_pid(pid);
    if (!w || !w->is_gfx) return -1;
    struct drawop o = { OP_CIRCLE, cx, cy, r, 0, color, {0} };
    return ring_push((int)(w - wins), &o);
}

/* 事件：gfx_task 压入（聚焦图形窗的键/点击），应用 sys28 轮询弹出 */
void gfx_ev_push(int i, int type, long a, long b) {
    cli();
    int nh = (ehead[i] + 1) % EVN;
    if (nh != etail[i]) {
        evq[i][ehead[i]].type = type;
        evq[i][ehead[i]].a = a;
        evq[i][ehead[i]].b = b;
        ehead[i] = nh;
    }
    sti();
}
int gfx_ev_pop(uint32_t pid, struct wmev *out) {
    struct win *w = win_by_pid(pid);
    if (!w || !w->is_gfx) return 0;
    int i = (int)(w - wins);
    cli();
    if (etail[i] == ehead[i]) { sti(); return 0; }
    out->type = evq[i][etail[i]].type;
    out->a = evq[i][etail[i]].a;
    out->b = evq[i][etail[i]].b;
    etail[i] = (etail[i] + 1) % EVN;
    sti();
    return 1;
}

/* gfx_task 侧：清底首帧 + 消费命令（单线程绘制纪律的图形窗半边） */
static void cv_char(int i, int x, int y, char c, uint32_t rgb) {
    struct win *w = &wins[i];
    const unsigned char *g = font8x8_basic[(int)c & 0x7F];
    uint32_t *cv = (uint32_t *)(uintptr_t)w->canvas;
    for (int j = 0; j < 8; j++) {
        int yy = y + j;
        if (yy < 0 || yy >= (int)w->h) continue;
        for (int k = 0; k < 8; k++) {
            int xx = x + k;
            if (xx >= 0 && xx < (int)w->w && (g[j] & (1u << k)))
                cv[yy * w->w + xx] = rgb;
        }
    }
}
static void op_apply(int i, const struct drawop *o) {
    struct win *w = &wins[i];
    if (!w->canvas) return;
    uint32_t *cv = (uint32_t *)(uintptr_t)w->canvas;
    int W = (int)w->w, H = (int)w->h;
    if (o->op == OP_RECT) {
        int x0 = o->x < 0 ? 0 : o->x, y0 = o->y < 0 ? 0 : o->y;
        int x1 = o->x + o->w > W ? W : o->x + o->w;
        int y1 = o->y + o->h > H ? H : o->y + o->h;
        for (int yy = y0; yy < y1; yy++)
            for (int xx = x0; xx < x1; xx++) cv[yy * W + xx] = o->color;
    } else if (o->op == OP_TEXT) {
        for (int k = 0; k < 48 && o->s[k]; k++)
            cv_char(i, o->x + k * 8, o->y, o->s[k], o->color);
    } else if (o->op == OP_LINE) {
        int dx = o->w - o->x, dy = o->h - o->y;      /* w,h 复用为终点 */
        int sx = dx > 0 ? 1 : -1, sy = dy > 0 ? 1 : -1;
        int adx = dx > 0 ? dx : -dx, ady = dy > 0 ? dy : -dy;
        int err = adx - ady, x = o->x, y = o->y;
        for (;;) {
            if (x >= 0 && x < W && y >= 0 && y < H) cv[y * W + x] = o->color;
            if (x == o->w && y == o->h) break;
            int e2 = err * 2;
            if (e2 > -ady) { err -= ady; x += sx; }
            if (e2 < adx)  { err += adx; y += sy; }
        }
    } else if (o->op == OP_CIRCLE) {
        int cx = o->x, cy = o->y, r = o->w;          /* 中点圆（八分对称） */
        int px = r, py = 0, err = 1 - r;
        while (px >= py) {
            int qx[8] = { cx + px, cx + py, cx - py, cx - px,
                          cx - px, cx - py, cx + py, cx + px };
            int qy[8] = { cy + py, cy + px, cy + px, cy + py,
                          cy - py, cy - px, cy - px, cy - py };
            for (int k = 0; k < 8; k++)
                if (qx[k] >= 0 && qx[k] < W && qy[k] >= 0 && qy[k] < H)
                    cv[qy[k] * W + qx[k]] = o->color;
            py++;
            if (err < 0) err += 2 * py + 1;
            else { px--; err += 2 * (py - px) + 1; }
        }
    }
}
static void ring_drain_all(void) {
    for (int i = 0; i < MAX_WIN; i++) {
        struct win *w = &wins[i];
        if (!w->used || !w->is_gfx || !w->canvas) continue;
        if (!w->canvas_ok) {
            memset((void *)(uintptr_t)w->canvas, 0, w->w * w->h * 4);
            w->canvas_ok = 1;
        }
        while (rtail[i] != rhead[i]) {
            struct drawop o = ring[i][rtail[i]];
            rtail[i] = (rtail[i] + 1) % RING_N;
            op_apply(i, &o);
        }
    }
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
    if (w->is_gfx) {
        /* 画布整块 blit（未清底先给黑，防垃圾帧闪现） */
        if (w->canvas_ok) {
            /* 行拷贝走 rep movsl：画布 896KB 逐像素拷在 TCG 下太慢 */
            const uint32_t *cv = (const uint32_t *)(uintptr_t)w->canvas;
            for (uint32_t j = 0; j < w->h; j++) {
                uint32_t *dst = (uint32_t *)(uintptr_t)
                                (fb_addr + (w->y + j) * fb_pitch) + w->x;
                const uint32_t *src = cv + j * w->w;
                uint32_t n = w->w;
                __asm__ volatile("cld; rep movsl" : "+c"(n), "+D"(dst), "+S"(src)
                                 :: "memory");
            }
        } else {
            fb_rect(w->x, w->y, w->w, w->h, 0x000000);
        }
    } else {
        fb_rect(w->x, w->y, w->w, w->h, C_WIN_BG);
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
    }
    fb_rect(w->x, w->y, w->w, 16, foc ? C_TITLE_FOC : C_TITLE_UNFOC);
    dtext(w->x + 3, w->y + 4, w->title, 0xFFFFFF, (w->w - 6) / 8);
    fb_rect(w->x + w->w - 15, w->y + 2, 13, 12, 0xC0392B);   /* 关闭钮 X */
    dtext(w->x + w->w - 13, w->y + 3, "X", 0xFFFFFF, 1);
    uint32_t bc = foc ? C_BORDER_FOC : C_BORDER_UNFOC;
    for (int t = 0; t < 2; t++) {
        fb_rect(w->x - t, w->y - t, w->w + 2 + 2 * t, 1, bc);
        fb_rect(w->x - t, w->y + w->h + t, w->w + 2 + 2 * t, 1, bc);
        fb_rect(w->x - t, w->y - t, 1, w->h + 2 + 2 * t, bc);
        fb_rect(w->x + w->w + t, w->y - t, 1, w->h + 2 + 2 * t, bc);
    }
}
static void draw_desktop(void) {
    fb_rect(0, 0, fb_w, fb_h, C_DESKTOP);
    fb_rect(0, fb_h - 28, fb_w, 28, C_TASKBAR);
    fb_rect(6, fb_h - 24, 90, 18, C_START);
    fb_text(14, fb_h - 20, "boot2xfce", 0x111111);
    fb_text(fb_w - 170, fb_h - 20, "ESC:new  click:focus", 0x8395A7);
    for (int k = 0; k < zn; k++)
        draw_window(zlist[k]);
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
/* 把槽位 i 升到叠放最顶（点击命中/新窗/复用时调） */
static void zraise(int i) {
    int k;
    for (k = 0; k < zn; k++) if (zlist[k] == i) break;
    if (k >= zn) return;
    for (; k < zn - 1; k++) zlist[k] = zlist[k + 1];
    zlist[zn - 1] = i;
}
/* 槽位 i 关闭后从叠放表摘除 */
static void zremove(int i) {
    int k;
    for (k = 0; k < zn; k++) if (zlist[k] == i) break;
    if (k >= zn) return;
    for (; k < zn - 1; k++) zlist[k] = zlist[k + 1];
    zn--;
}
static int win_new(const char *title, uint32_t pid, int is_tty) {
    /* 同属主已有窗口则直接复用（一个任务一个窗），并升到最前 */
    if (pid) {
        struct win *ex = win_by_pid(pid);
        if (ex) { int i = (int)(ex - wins); focus = i; zraise(i); return i; }
    }
    for (int i = 0; i < MAX_WIN; i++) {
        if (wins[i].used) continue;
        canvas_free(i);                      /* 防御：槽位残留画布先释放 */
        memset(&wins[i], 0, sizeof(wins[i]));
        rhead[i] = rtail[i] = 0;
        ehead[i] = etail[i] = 0;
        wins[i].used = 1;
        wins[i].pid = pid;
        wins[i].is_tty = is_tty;
        for (int j = 0; j < 23 && title[j]; j++) wins[i].title[j] = title[j];
        wins[i].x = 30 + (i % 2) * 90;
        wins[i].y = 24 + (i / 2) * 60;
        wins[i].w = 560;
        wins[i].h = 400;
        zlist[zn++] = i;                     /* 新窗落最顶 */
        focus = i;
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
            if (*s != '\n') w->input[w->ilen++] = *s;
        } else w->input[w->ilen++] = *s;
    }
}

/* ---------- 应用/任务联动（sys 22 spawn / 窗口跟随） ----------
 * 绘制纪律：帧缓冲只允许 gfx_task 单线程绘制（IRQ0 抢占下，spawn/execve
 * 的系统调用上下文若直接 draw_desktop 会与 gfx_task 的重绘交错出撕裂帧）。
 * 其他上下文只在 cli 临界区里改 wins[] 并推进 gfx_gen。 */
int gfx_spawn_window(uint32_t pid, const char *title) {
    cli();
    int i = win_new(title, pid, 1);
    gfx_gen++;
    sti();
    return i;
}
void gfx_on_exit(uint32_t pid) {                     /* 任务退出关窗 */
    cli();
    for (int i = 0; i < MAX_WIN; i++)
        if (wins[i].used && wins[i].pid == pid) {
            canvas_free(i);
            zremove(i);
            wins[i].used = 0;
        }
    gfx_gen++;
    sti();
}

/* ---------- tty 联动（tty.c 调用） ---------- */
static struct wtty { int used, ready, len; char line[120]; } wt[MAX_WIN];
static struct wtty legacy;                           /* gfx 前的全局输入 */
static int legacy_on = 1;

void tty_feed_global(char c) {                       /* gfx 前的键入 */
    if (c == '\n' || (legacy.len && legacy.len < 119)) {
        if (c != '\n') legacy.line[legacy.len++] = c;
        legacy.ready = 1;
    }
}
int tty_read_global(char *out, int max) {
    while (!legacy.ready) { current->if_on = 0; schedule(); }
    int n = legacy.len < max ? legacy.len : max - 1;
    memcpy(out, legacy.line, n); out[n] = 0;
    legacy.len = 0; legacy.ready = 0;
    return n;
}
int win_by_pid_ok(uint32_t pid) {
    return win_by_pid(pid) != 0;
}
void gfx_clear_focused(void) {
    struct win *w = win_by_pid(current->pid);
    if (!w) return;
    memset(w->lines, 0, sizeof(w->lines));
    w->top = 0; w->nl = 0;
    memset(w->input, 0, sizeof(w->input));
    w->ilen = 0;
    gfx_gen++;
}

void gfx_console_write(uint32_t pid, const char *s, uint32_t n) {
    struct win *w = win_by_pid(pid);
    if (!w) return;
    char tmp[256];
    uint32_t k = n < 255 ? n : 255;
    memcpy(tmp, s, k); tmp[k] = 0;
    win_puts((int)(w - wins), tmp);
    gfx_gen++;
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
    { extern void debug_ser(const char *);
      extern void debug_hex(uint32_t);
      debug_ser("[k="); debug_hex((uint32_t)c); debug_ser("]\n"); }
    struct wtty *t = focus_wt();
    if (!t) return;
    if (c == '\b') { if (t->len) { t->len--; struct win *w = &wins[focus]; if (w->ilen) { w->ilen--; w->input[w->ilen] = 0; } } return; }
    if (c == '\n') { t->ready = 1; return; }
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
        current->if_on = 0;             /* 阻塞轮询发生在 int80 门内 */
        if (t->ready) {
            { extern void debug_ser(const char *);
              debug_ser("[wt wake]\n"); }
            int n = t->len < max ? t->len : max - 1;
            memcpy(out, t->line, n); out[n] = 0;
            t->len = 0; t->ready = 0;
            /* 输入行进内容区 */
            win_puts((int)(w - wins), "\n");
            return n;
        }
        schedule();
    }
}

/* ---------- 启动/事件 ---------- */
void gfx_start(void) {
    legacy_on = 0;
    /* 先建窗再放行：gfx_task 首绘时窗口必须已在，免得等到首个键鼠事件才出现 */
    extern task_t *current;
    cli();
    win_new("shell", current->pid, 1);
    gfx_gen++;
    sti();
    gfx_on = 1;
}

static int spawn_shell(void) {
    extern int task_spawn(const char *path, uint32_t ppid, const char *title);
    return task_spawn("/bin/sh", 0, "shell");
}

void gfx_task(void);
void gfx_task(void) {
    static int fb_ready = 0;
    static unsigned drawn_gen = 0;      /* 已上屏的内容代数 */
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
        /* 鼠标事件：攒一批，末尾统一重绘 */
        int moved = 0;
        uint32_t close_pid = 0;
        while (mouse_pop(&btn, &dx, &dy)) {
            cursor_erase();
            cur_x += dx; cur_y -= dy;
            if (cur_x < 0) cur_x = 0; if (cur_x > (int)fb_w - 9) cur_x = (int)fb_w - 9;
            if (cur_y < 0) cur_y = 0; if (cur_y > (int)fb_h - 13) cur_y = (int)fb_h - 13;
            if (btn & 1) {
                /* 命中测试按 Z 序从顶往下；命中即升至最前（点哪层哪层到前台） */
                for (int k = zn - 1; k >= 0; k--) {
                    int i = zlist[k];
                    struct win *w = &wins[i];
                    if (!w->used) continue;
                    if (cur_x >= (int)w->x && cur_x < (int)(w->x + w->w)
                     && cur_y >= (int)w->y && cur_y < (int)(w->y + w->h)) {
                        if (cur_x >= (int)(w->x + w->w - 15) && cur_x < (int)(w->x + w->w - 2)
                         && cur_y >= (int)w->y + 2 && cur_y < (int)w->y + 14) {
                            close_pid = w->pid;   /* 关闭钮：批末终止归属任务 */
                        } else {
                            zraise(i);            /* 点击升到前台 */
                            focus = i;
                            if (w->is_gfx)        /* 图形窗收点击事件 */
                                gfx_ev_push(i, 2,
                                            (long)(cur_x - (int)w->x),
                                            (long)(cur_y - (int)w->y));
                        }
                        break;
                    }
                }
            }
            moved = 1;
        }
        if (close_pid) task_kill(close_pid);      /* 关窗：终止应用并回收 */
        if (moved) { draw_desktop(); cursor_draw(); }
        /* 键盘事件 */
        int c, keyed = 0;
        while ((c = kbd_pop()) >= 0) {
            if (c == 27) {                       /* ESC：新 shell 窗口 */
                int pid = spawn_shell();
                if (pid) gfx_spawn_window((uint32_t)pid, "shell");
                keyed = 1;
                continue;
            }
            {
                struct win *w = &wins[focus];
                if (w->used && w->is_gfx)
                    gfx_ev_push(focus, 1, c, 0); /* 图形窗收原始键 */
                else if (c == '\n' || (c >= 32 && c < 127) || c == 8)
                    gfx_key_feed((char)c);
            }
            keyed = 1;
        }
        ring_drain_all();                        /* 图形窗：清底首帧+消费命令 */
        /* 输出驱动刷新：以代数比较代替 test-and-clear 标志——布尔 dirty
         * 会被并发绘制轮清掉（shell 在别任务上下文置脏的同一瞬间），丢一次
         * 更新就永久停在旧帧；代数只增不减，漏不掉任何一轮改动。 */
        gfx_passes++;
        if (keyed || gfx_gen != drawn_gen) {
            draw_desktop();
            if (cur_on) cursor_draw();
            drawn_gen = gfx_gen;
            gfx_draws++;
        }
        task_yield();
    }
}
