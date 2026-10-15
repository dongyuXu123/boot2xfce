/* mouse.c —— PS/2 辅设（鼠标）驱动：IRQ12 三字节包 → 事件环
 * 包格式：b0[0]=左键 b0[1]=右键 b0[3]=恒1 b0[4]=x符号 b0[5]=y符号
 *         b1=x增量 b2=y增量（y 与屏幕方向相反）
 */
#include "kernel.h"

struct mev { uint8_t btn; int16_t dx, dy; };
#define MEV_MAX 64
volatile unsigned mi_cnt, mp_cnt;
static struct mev mring[MEV_MAX];
static int mh, mt;
static uint8_t pk[3];
static int pkn;

static void aux_wait_in(void) { for (int i = 0; i < 100000 && (inb(0x64) & 2); i++); }
static void aux_wait_out(void) { for (int i = 0; i < 100000 && !(inb(0x64) & 1); i++); }
static void aux_write(uint8_t b) {
    aux_wait_in(); outb(0x64, 0xD4); aux_wait_in(); outb(0x60, b);
}
/* 排干输出缓冲：ACK/自检字节数量与时序不定（QEMU 异步投递），按数硬吞
 * 会残留滞留字节甚至吞失败——启用上报(0xF4)实际没生效，鼠标从此静默 */
static void aux_flush(void) {
    for (int i = 0; i < 200000 && (inb(0x64) & 1); i++) inb(0x60);
}

void mouse_init(void) {
    aux_wait_in(); outb(0x64, 0xA8);            /* 启用辅设 */
    aux_wait_in(); outb(0x64, 0x20);            /* 读命令字节 */
    aux_wait_out();
    uint8_t cfg = inb(0x60) | 2;                /* 开 IRQ12 与时钟 */
    aux_wait_in(); outb(0x64, 0x60);
    aux_wait_in(); outb(0x60, cfg);
    aux_write(0xF6);                            /* 默认参数 */
    aux_write(0xF4);                            /* 开上报 */
    aux_flush();                                /* 排干 ACK/自检滞留 */
    /* 从片开 IRQ12 */
    outb(0xA1, (inb(0xA1) & ~(1u << 4)));
}

static void push(uint8_t btn, int16_t dx, int16_t dy) {
    int nx = (mh + 1) % MEV_MAX;
    if (nx == mt) return;
    mring[mh].btn = btn; mring[mh].dx = dx; mring[mh].dy = dy;
    mh = nx;
}

void mouse_intr(void) {
    mi_cnt++;
    uint8_t st = inb(0x64);
    if (!(st & 1)) return;
    uint8_t d = inb(0x60);
    if (!(st & 0x20)) return;                   /* 不是鼠标数据 */
    if (pkn == 0 && (d & 0xC0)) return;         /* 字节0 高两位必须为 0：否则失步，丢弃重对齐 */
    pk[pkn++] = d;
    if (pk[0] & 8) {                            /* 同步字节对齐 */
        if (pkn == 3) {
            pkn = 0;
            int16_t dx = pk[1], dy = pk[2];
            if (pk[0] & 0x10) dx -= 256;
            if (pk[0] & 0x20) dy -= 256;
            push(pk[0] & 7, dx, dy);
        }
    } else pkn = 0;
}

int mouse_pop(uint8_t *btn, int16_t *dx, int16_t *dy) {
    if (mh != mt) mp_cnt++;
    /* 同 kbd_pop：关中断临界区防 IRQ12 压入与任务弹出交错丢事件 */
    uint32_t fl;
    __asm__ volatile("pushfl; popl %0; cli" : "=r"(fl));
    if (mh == mt) { __asm__ volatile("pushl %0; popfl" :: "r"(fl)); return 0; }
    *btn = mring[mt].btn; *dx = mring[mt].dx; *dy = mring[mt].dy;
    mt = (mt + 1) % MEV_MAX;
    __asm__ volatile("pushl %0; popfl" :: "r"(fl));
    return 1;
}
