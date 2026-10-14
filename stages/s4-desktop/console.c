/* console.c —— VGA 文本控制台，照 Linux drivers/tty/vt/vt.c 的模型实现
 *
 * 对照（upstream/linux 同名符号）：
 *   struct vc_data          → struct vc        （列/行/光标 x,y/属性/转义状态机）
 *   scrup()                 → scrup()           （底部整体上滚，vt.c SM_UP）
 *   gotoxy()                → gotoxy()          （边界钳制，vt.c:1430）
 *   lf()/cr()/bs()          → 同名              （vt.c:1479-1518）
 *   csi_J()/csi_K()         → 同名              （ED/EL 清屏，vt.c:1537/1580）
 *   vc_con_write_con() 的 ESC 解析 → vc_putc() 状态机（ESnormal/ESesc/ESsquare…）
 *   set_cursor()            → 同名              （写 VGA CRTC 硬件光标，会闪烁）
 *
 * 输出双路：本控制台 + 串口（调试观测用）。
 */
#include "kernel.h"

#define VC_COLS 80
#define VC_ROWS 25
#define VGA_MEM ((volatile uint16_t *)0xB8000)
#define COM1    0x3F8

/* ---------- vc_data（vt.c struct vc_data 的教学子集） ---------- */
static struct vc {
    int  cols, rows;
    int  x, y;              /* state.x / state.y（光标逻辑坐标） */
    uint16_t attr;          /* 当前属性字节（默认 0x07，vt.c default_attr） */
    uint16_t erased;        /* attr<<8|' '，csi_J/csi_K/滚屏用它清（video_erase_char） */
    /* ESC 解析状态机（vt.c vc_state: ESnormal/ESesc/ESsquare/ESgetpars/ESgotpars） */
    enum { ESnormal, ESesc, ESsquare, ESgetpars, ESgotpars } state;
    int  npar;              /* vc_npar */
    int  par[8];            /* vc_par[8] */
    int  priv;              /* ESC [ ? 的 DEC 私有序列标志（vt.c vc_priv） */
} vc = { VC_COLS, VC_ROWS, 0, 0, 0x07 };

static uint16_t vc_origin_row0(void) { return 0; }

/* ---------- 串口（调试观测通道） ---------- */
static void serial_init(void) {
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1,     0x01);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}
static void serial_putc(char c) {
    while (!(inb(COM1 + 5) & 0x20)) ;
    outb(COM1, (uint8_t)c);
}

/* ---------- 硬件光标（vt.c set_cursor → VGA CRTC 寄存器） ---------- */
#define CRTC_IDX 0x3D4
#define CRTC_DAT 0x3D5
static void crt_write(uint8_t idx, uint8_t val) {
    outb(CRTC_IDX, idx);
    outb(CRTC_DAT, val);
}
static void set_cursor(void) {
    uint16_t pos = vc.y * vc.cols + vc.x;      /* vt.c: vc_pos 换算成字符格 */
    crt_write(0x0E, (uint8_t)(pos >> 8));      /* CRTC 0x0E: 光标位置高字节 */
    crt_write(0x0F, (uint8_t)(pos & 0xFF));    /* CRTC 0x0F: 光标位置低字节 */
}
static void hide_cursor(void) {                /* vt.c hide_cursor: 置 bit5 关显 */
    crt_write(0x0A, 0x20);
}
static void cursor_shape(void) {               /* 扫描行 0..13 → 硬件自动闪烁 */
    crt_write(0x0A, 0x00);                     /* bit5=0 显示；起行 0 */
    crt_write(0x0B, 0x0D);                     /* 止行 13（8x16 字格内一条下划线样式） */
}

/* ---------- 滚动（vt.c con_scroll SM_UP/SM_DOWN） ---------- */
static void scrup(int nr) {                    /* 整体上滚 nr 行 */
    for (int y = 0; y < VC_ROWS - nr; y++)
        for (int x = 0; x < VC_COLS; x++)
            VGA_MEM[y * VC_COLS + x] = VGA_MEM[(y + nr) * VC_COLS + x];
    for (int y = VC_ROWS - nr; y < VC_ROWS; y++)
        for (int x = 0; x < VC_COLS; x++)
            VGA_MEM[y * VC_COLS + x] = vc.erased;
}
static void scrdown(int nr) {                  /* 整体下滚（ESC M 逆行用，vt.c SM_DOWN） */
    for (int y = VC_ROWS - 1; y >= nr; y--)
        for (int x = 0; x < VC_COLS; x++)
            VGA_MEM[y * VC_COLS + x] = VGA_MEM[(y - nr) * VC_COLS + x];
    for (int y = 0; y < nr; y++)
        for (int x = 0; x < VC_COLS; x++)
            VGA_MEM[y * VC_COLS + x] = vc.erased;
}

/* ---------- 光标定位（vt.c gotoxy:1430 边界钳制） ---------- */
static void gotoxy(int nx, int ny) {
    vc.x = (nx < 0) ? 0 : (nx >= vc.cols ? vc.cols - 1 : nx);
    vc.y = (ny < 0) ? 0 : (ny >= vc.rows ? vc.rows - 1 : ny);
}

/* ---------- 控制字符（vt.c lf:1479 / cr:1507 / bs:1511） ---------- */
static void lf(void) {
    if (vc.y + 1 == vc.rows) scrup(1);         /* 底行：整体上滚（vt.c con_scroll SM_UP） */
    else vc.y++;
}
static void cr(void)  { vc.x = 0; }
static void bs(void)  { if (vc.x) vc.x--; }

/* ---------- csi_K（vt.c:1580 行内擦除） ---------- */
static void csi_K(int vpar) {
    if (vpar == 0)                             /* 光标到行尾 */
        for (int x = vc.x; x < vc.cols; x++)
            VGA_MEM[vc.y * VC_COLS + x] = vc.erased;
    else if (vpar == 1)                        /* 行首到光标 */
        for (int x = 0; x <= vc.x; x++)
            VGA_MEM[vc.y * VC_COLS + x] = vc.erased;
    else                                       /* 整行 */
        for (int x = 0; x < vc.cols; x++)
            VGA_MEM[vc.y * VC_COLS + x] = vc.erased;
}
/* ---------- csi_J（vt.c:1537 ED 屏幕擦除，0/1/2 同 Linux 语义） ---------- */
static void csi_J(int vpar) {
    if (vpar == 0) {
        csi_K(0);
        for (int y = vc.y + 1; y < vc.rows; y++)
            for (int x = 0; x < vc.cols; x++)
                VGA_MEM[y * VC_COLS + x] = vc.erased;
    } else if (vpar == 1) {
        for (int y = 0; y < vc.y; y++)
            for (int x = 0; x < vc.cols; x++)
                VGA_MEM[y * VC_COLS + x] = vc.erased;
        for (int x = 0; x <= vc.x; x++)
            VGA_MEM[vc.y * VC_COLS + x] = vc.erased;
    } else {
        for (int i = 0; i < VC_COLS * VC_ROWS; i++) VGA_MEM[i] = vc.erased;
        gotoxy(0, 0);
    }
}
/* ---------- csi_m（vt.c:1752 SGR 颜色，30-37/40-47/1/0） ---------- */
static void csi_m(int v) {
    if (v == 0)      vc.attr = 0x07;                 /* reset */
    else if (v == 1) vc.attr |= 0x08;                /* 亮色位 */
    else if (v >= 30 && v <= 37) {
        vc.attr = (uint16_t)((vc.attr & 0xF8) | ((v - 30) & 0x07) | ((vc.attr & 0x08)));
    } else if (v >= 40 && v <= 47) {
        vc.attr = (uint16_t)((vc.attr & 0x8F) | (((v - 40) & 0x07) << 4));
    }
}

/* ---------- CSI 分发（vt.c:2590 'H'/'J'/'K'/'m'/'A'-'D' 同表） ---------- */
static void vc_do_csi(char c) {
    int p0 = vc.par[0], p1 = vc.par[1];
    switch (c) {
    case 'H': case 'f':                          /* 光标定位（1 基 → 0 基） */
        gotoxy(p1 ? p1 - 1 : 0, p0 ? p0 - 1 : 0);
        break;
    case 'J': csi_J(p0); break;
    case 'K': csi_K(p0); break;
    case 'm': csi_m(p0); break;
    case 'A': gotoxy(vc.x, vc.y - (p0 ? p0 : 1)); break;   /* 上 */
    case 'B': gotoxy(vc.x, vc.y + (p0 ? p0 : 1)); break;   /* 下 */
    case 'C': gotoxy(vc.x + (p0 ? p0 : 1), vc.y); break;   /* 右 */
    case 'D': gotoxy(vc.x - (p0 ? p0 : 1), vc.y); break;   /* 左 */
    case 'E': cr(); lf(); break;                            /* 下一行行首 */
    default: break;                                          /* 其余忽略（vt.c 同样宽容） */
    }
}

/* ---------- 写一个字符（vt.c vc_con_write_con 主路径） ---------- */
static void vc_putc(char c) {
    switch (vc.state) {
    case ESesc:                                  /* 收到 ESC 后的下一字符 */
        vc.state = ESnormal;
        switch (c) {
        case '[': vc.npar = 0; vc.priv = 0;
                  for (int i = 0; i < 8; i++) vc.par[i] = 0;
                  vc.state = ESsquare; return;
        case 'E': cr(); lf(); return;            /* NEL */
        case 'M': {                              /* 反向换行（vt.c ri） */
            if (vc.y == 0) scrdown(1); else vc.y--;
            return; }
        case 'D': lf(); return;                  /* IND */
        default: return;
        }
    case ESsquare:                               /* CSI：收集参数 */
        if (c >= '0' && c <= '9') {
            vc.par[vc.npar] = vc.par[vc.npar] * 10 + (c - '0');
            vc.state = ESgetpars;
            return;
        }
        if (c == '?') { vc.priv = 1; return; }   /* DEC 私有前缀（记录但忽略） */
        if (c == ';') { if (vc.npar < 7) vc.npar++; return; }
        vc.state = ESnormal;
        vc_do_csi(c);
        return;
    case ESgetpars:                              /* 多位参数继续 */
        if (c >= '0' && c <= '9') {
            if (vc.npar < 8) vc.par[vc.npar] = vc.par[vc.npar] * 10 + (c - '0');
            return;
        }
        if (c == ';') { if (vc.npar < 7) vc.npar++; return; }
        vc.state = ESnormal;
        vc_do_csi(c);
        return;
    default:                                     /* ESnormal */
        break;
    }
    switch (c) {
    case 27:  vc.state = ESesc; return;          /* ESC */
    case '\n': lf(); return;
    case '\r': cr(); return;
    case '\b': bs(); return;
    case '\t': {                                 /* 制表：到 8 的倍数（vt.c tab 位图语义） */
        int nx = (vc.x | 7) + 1;
        gotoxy(nx, vc.y);
        return; }
    case '\f':                                   /* FF：清屏回位（clear 的另一形式） */
        csi_J(2);
        return;
    default:
        if (c < 32) return;                      /* 其余控制字符忽略（vt.c 同） */
        /* 可打印：写 attr+char，行满置 need_wrap → 下字符先换行 */
        VGA_MEM[vc.y * VC_COLS + vc.x] = (uint16_t)(vc.attr << 8 | (uint8_t)c);
        if (vc.x == vc.cols - 1) { vc.x = vc.cols; lf(); vc.x = 0; }
        else vc.x++;
        return;
    }
}

/* ---------- 内存原语（原 console.c 提供；kernel.h 声明） ---------- */
void *memset(void *d, int c, uint32_t n) {
    uint8_t *p = d;
    while (n--) *p++ = (uint8_t)c;
    return d;
}
void *memcpy(void *d, const void *s, uint32_t n) {
    uint8_t *p = d;
    const uint8_t *q = s;
    while (n--) *p++ = *q++;
    return d;
}

/* ---------- 对外接口 ---------- */
void console_write(const char *s, uint32_t n) {  /* print() 的落点 */
    {   /* gfx 模式：当前任务若有窗口，输出进窗口（vt 不可见） */
        extern int gfx_active(void);
        extern void gfx_console_write(uint32_t, const char *, uint32_t);
        extern task_t *current;
        if (gfx_active() && current && win_by_pid_ok(current->pid)) {
            gfx_console_write(current->pid, s, n);
            return;
        }
    }
    for (uint32_t i = 0; i < n; i++) {
        vc_putc(s[i]);
        serial_putc(s[i]);
    }
    set_cursor();
}
int win_by_pid_ok(uint32_t pid);                 /* win.c：有无属主窗口 */
void console_init(void) {
    serial_init();
    cursor_shape();                              /* 硬件光标：下划线样式、自动闪烁 */
    csi_J(2);
    gotoxy(0, 0);
    set_cursor();
}
void console_clear(void) {                       /* sh 的 clear：等价 ESC[2J ESC[H */
    csi_J(2);
    set_cursor();
}
int  console_cursor_row(void) { return vc.y; }

/* ---------- print 家族（kernel.h 接口；串口双路已在 console_write 内） ---------- */
void print(const char *s)            { uint32_t n = 0; while (s[n]) n++; console_write(s, n); }
void print_hex(uint32_t v) {
    static const char h[] = "0123456789abcdef";
    char b[11] = "0x";
    for (int i = 0; i < 8; i++) b[2+i] = h[(v >> ((7-i)*4)) & 0xF];
    console_write(b, 10);
}
void print_dec(uint32_t v) {
    char b[11]; int i = 10; b[i] = 0;
    do { b[--i] = (char)('0' + v % 10); v /= 10; } while (v);
    console_write(&b[i], 10 - i);
}
