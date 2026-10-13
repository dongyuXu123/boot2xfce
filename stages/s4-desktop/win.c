/* win.c —— S4：极简窗口系统 + 键盘驱动桌面演示
 * 桌面+任务栏+窗口（标题栏/装饰条/内容/焦点边框）。
 * 键盘：Tab 切焦点、a/d 移动、n 开新窗。
 * 进入：shell 输 gfx（SYS 17）→ gfx 内核任务接管屏幕与键盘。
 */
#include "kernel.h"

#define C_DESKTOP      0x1E3A5F
#define C_TASKBAR      0x14202E
#define C_TITLE_FOC    0x2E86DE
#define C_TITLE_UNFOC  0x57606F
#define C_BORDER_FOC   0xF9CA24
#define C_BORDER_UNFOC 0x353B48
#define C_WIN_BG       0xEEEEEE
#define C_TEXT         0x111111
#define C_START        0x4CD137

#define MAX_WIN 4
struct win {
    int used;
    uint32_t x, y, w, h;
    const char *title;
    const char *body[6];
    uint32_t rgb;
};
static struct win wins[MAX_WIN];
static int focus = 0;
static int gfx_on = 0;

int gfx_active(void) { return gfx_on; }

static void draw_window(struct win *w, int focused) {
    fb_rect(w->x, w->y, w->w, w->h, C_WIN_BG);
    fb_rect(w->x, w->y, w->w, 20, focused ? C_TITLE_FOC : C_TITLE_UNFOC);
    fb_text(w->x + 4, w->y + 6, w->title, 0xFFFFFF);
    fb_rect(w->x + 2, w->y + 22, w->w - 4, 10, w->rgb);
    for (int i = 0; i < 6 && w->body[i]; i++)
        fb_text(w->x + 8, w->y + 40 + i * 10, w->body[i], C_TEXT);
    uint32_t bc = focused ? C_BORDER_FOC : C_BORDER_UNFOC;
    for (int t = 0; t < 2; t++) {
        fb_rect(w->x - t, w->y - t, w->w + 2 + 2 * t, 1, bc);
        fb_rect(w->x - t, w->y + w->h + t, w->w + 2 + 2 * t, 1, bc);
        fb_rect(w->x - t, w->y - t, 1, w->h + 2 + 2 * t, bc);
        fb_rect(w->x + w->w + t, w->y - t, 1, w->h + 2 + 2 * t, bc);
    }
}
static void draw_desktop(void) {
    fb_rect(0, 0, fb_w, fb_h, C_DESKTOP);
    fb_rect(0, fb_h - 32, fb_w, 32, C_TASKBAR);
    fb_rect(6, fb_h - 26, 90, 20, C_START);
    fb_text(14, fb_h - 22, "boot2xfce", 0x111111);
    fb_text(fb_w - 150, fb_h - 22, "S4 Track B", 0x8395A7);
    for (int i = 0; i < MAX_WIN; i++)
        if (wins[i].used) draw_window(&wins[i], i == focus);
    fb_text(8, 8, "gfx: [Tab] focus  [a/d] move  [n] new win", C_BORDER_FOC);
}

static void win_add(const char *title, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t rgb) {
    for (int i = 0; i < MAX_WIN; i++) {
        if (wins[i].used) continue;
        wins[i] = (struct win){ 1, x, y, w, h, title, { 0 }, rgb };
        focus = i;
        return;
    }
}
static void win_body(int idx, int line, const char *s) {
    if (idx >= 0 && idx < MAX_WIN && wins[idx].used && line < 6)
        wins[idx].body[line] = s;
}

void gfx_start(void) {
    /* 只置位：fb_init/绘制由 gfx 内核任务执行 */
    gfx_on = 1;
}

static int fb_ready = 0;
static void gfx_activate(void) {
    fb_init();
    win_add("about", 40, 60, 300, 130, 0x2E86DE);
    win_body(0, 0, "boot2xfce Track B");
    win_body(0, 1, "kernel-made window system");
    win_body(0, 2, "no X11, no compositor,");
    win_body(0, 3, "just our own framebuffer.");
    win_add("clock", 380, 100, 210, 90, 0xEE5A24);
    win_body(1, 0, "uptime ticks below:");
    win_add("keyboard", 120, 240, 340, 110, 0x10AC84);
    win_body(2, 0, "gfx owns the keyboard now.");
    draw_desktop();
    fb_blit();
    fb_ready = 1;
}

static void gfx_key(char c) {
    if (c == '\t') {
        int n = focus, tries = 0;
        do { n = (n + 1) % MAX_WIN; tries++; } while (!wins[n].used && tries < MAX_WIN);
        focus = n;
    } else if (c == 'a' && wins[focus].x > 8) {
        wins[focus].x -= 12;
    } else if (c == 'd' && wins[focus].x + wins[focus].w < fb_w - 8) {
        wins[focus].x += 12;
    } else if (c == 'n') {
        win_add("new", 200, 180, 220, 80, 0x9B59B6);
        win_body(focus, 5, "fresh window!");
    } else {
        return;
    }
    draw_desktop();
    fb_blit();
}

void gfx_task(void) {
    static uint32_t last_ticks = 0;
    for (;;) {
        if (!gfx_active()) { task_yield(); continue; }
        if (!fb_ready) { gfx_activate(); task_yield(); continue; }
        int c = kbd_pop();
        if (c == '\t' || (c >= 'a' && c <= 'z')) gfx_key((char)c);
        if (ticks - last_ticks >= 100) {
            last_ticks = ticks;
            if (wins[1].used) {
                static char l2[24];
                const char *p = "ticks: ";
                int i = 0;
                for (; p[i]; i++) l2[i] = p[i];
                uint32_t v = ticks, d = 100000000;
                while (d) { l2[i++] = (char)('0' + v / d); v %= d; d /= 10; }
                l2[i] = 0;
                wins[1].body[1] = l2;
                draw_desktop();
                fb_blit();
            }
        }
        task_yield();
    }
}
