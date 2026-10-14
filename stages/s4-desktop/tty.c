/* tty.c —— S3-M6：行规程 + /dev/tty
 * 键盘中断只产字符；行规程负责编辑（退格）与回显，攒成"一行"再交给读者。
 * tty_read 的阻塞是教学版：没有睡眠队列，就"schedule()+重查"。
 */
#include "kernel.h"

#define LINE_MAX 128
static char   line[LINE_MAX];
static int    line_len;                     /* 已就绪的行长度（含\n 则可读） */
static int    line_ready;
static struct vnode tty_vnode;

/* kbd_intr 之后由 irq 分发调用：把字符喂进行规程 */
void tty_feed(char c) {
    if (c == '\b') {                        /* 退格：抹掉上一个字符 */
        if (line_len > 0) { line_len--; print("\b \b"); }
        return;
    }
    if (c == '\r') c = '\n';
    if (line_len < LINE_MAX - 1) {
        line[line_len++] = c;
        { char b[2] = { c, 0 }; print(b); }  /* 回显 */
    }
    if (c == '\n') line_ready = 1;
}

static int tty_read_impl(struct vnode *v, void *buf, uint32_t len, uint32_t off) {
    (void)v; (void)off;
    /* gfx 桌面：shell 的 stdin 改走本窗口行规程（win.c wt[]）。
     * 旧全局行规程在 gfx 下必须停用——否则读者与 gfx_task 抢 kbd_pop，
     * 且这里等的 line_ready 在 gfx 下永远不会被置位（回车"失灵"）。 */
    if (gfx_active()) return tty_read_window(current->pid, (char *)buf, (int)len);
    /* 阻塞：行没攒好就让出 CPU（无睡眠队列的教学实现） */
    while (!line_ready) {
        int c = kbd_pop();
        if (c >= 32 && c < 127) tty_feed((char)c);
        else if (c == '\b') tty_feed((char)c);
        else if (c == '\n') tty_feed((char)'\n');
        schedule();
    }
    if (len > (uint32_t)line_len) len = (uint32_t)line_len;
    memcpy(buf, line, len);
    line_len = 0;
    line_ready = 0;
    return (int)len;
}
static int tty_write_impl(struct vnode *v, const void *buf, uint32_t len, uint32_t off) {
    (void)v; (void)off;
    if (gfx_active()) {
        char tmp[256];
        uint32_t n2 = len < 255 ? len : 255;
        memcpy(tmp, buf, n2); tmp[n2] = 0;
        tty_write_window(current->pid, tmp);
        return (int)n2;
    }
    const char *s = buf;
    for (uint32_t i = 0; i < len; i++) { char b[2] = { s[i], 0 }; print(b); }
    return (int)len;
}
static const struct file_ops tty_ops = { tty_read_impl, tty_write_impl };

struct vnode *tty_lookup_vnode(void) {
    tty_vnode.ops = &tty_ops;
    tty_vnode.data = 0;
    return &tty_vnode;
}

void tty_init(void) { line_len = 0; line_ready = 0; }

/* 键盘软链：kbd 环形缓冲 → 行规程（在 IRQ1 分发里被调） */
void tty_poll(void) {
    if (gfx_active()) return;
    int c;
    while ((c = kbd_pop()) >= 0) {
        if (c == '\b' || c == '\n' || (c >= 32 && c < 127)) tty_feed((char)c);
    }
}
