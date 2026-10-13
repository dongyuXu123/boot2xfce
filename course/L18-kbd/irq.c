/* L18 桩 —— 键盘 IRQ1（PIC/PIT 是你 L11 的成果，保留）
 * 你写 kbd_intr：读通码、过滤释放码、查表转 ASCII、进环形缓冲。
 * 验收是硬核的：make test 会用 QEMU monitor 真注一个 'a' 键，
 * 你的 kbd 任务必须把它回显成 "kbd: a"。 */
#include "kernel.h"

volatile uint32_t ticks = 0;
volatile uint32_t isr_err_code = 0;

/* ---------- 8259A（你的 L11 成果） ---------- */
#define PIC1_CMD 0x20
#define PIC1_DAT 0x21
#define PIC2_CMD 0xA0
#define PIC2_DAT 0xA1

void pic_init(void) {
    outb(PIC1_CMD, 0x11);
    outb(PIC2_CMD, 0x11);
    outb(PIC1_DAT, 0x20);
    outb(PIC2_DAT, 0x28);
    outb(PIC1_DAT, 0x04);
    outb(PIC2_DAT, 0x02);
    outb(PIC1_DAT, 0x01);
    outb(PIC2_DAT, 0x01);
    outb(PIC1_DAT, 0xF8);
    outb(PIC2_DAT, 0xFF);
}

/* ---------- PIT（你的 L11 成果） ---------- */
void pit_init(uint32_t hz) {
    uint32_t div = 1193182 / hz;
    outb(0x43, 0x36);
    outb(0x40, div & 0xFF);
    outb(0x40, (div >> 8) & 0xFF);
}

/* ---------- 键盘：本课你的作业 ---------- */
static uint8_t kbuf[32];
static int kbuf_head, kbuf_tail;

static const char sc2ascii[59] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=','\b','\t',
    'q','w','e','r','t','y','u','i','o','p','[',']','\n', 0,
    'a','s','d','f','g','h','j','k','l',';','\'','`', 0, '\\',
    'z','x','c','v','b','n','m',',','.','/', 0, '*', 0, ' '
};

static void kbd_intr(void) {
    /* TODO(你来写·L18):
     * 1) inb(0x60) 读通码（这一读同时是对键盘控制器的应答）
     * 2) 最高位为 1 → 是释放码，忽略返回
     * 3) 查 sc2ascii 表（越界或 0 跳过）
     * 4) 环形缓冲不满就入队：kbuf[head]=c; head=(head+1)%32
     * 想想：为什么判满用 next != tail 而不是 head == tail？ */
}

int kbd_pop(void) {
    if (kbuf_head == kbuf_tail) return -1;
    int c = kbuf[kbuf_tail];
    kbuf_tail = (kbuf_tail + 1) % 32;
    return c;
}

/* ---------- 异常现场转储（给定） ---------- */
static void fault_dump(struct regs *r) {
    cli();
    print("\nS2: FAULT int_no=");
    print_dec(r->int_no);
    print(" eip=");
    print_hex(r->eip);
    print(" esp=");
    print_hex(r->esp_);
    print(" cs=");
    print_hex(r->cs);
    print(" err=");
    print_hex(isr_err_code);
    if (r->int_no == 14) { print(" cr2="); print_hex(read_cr2()); }
    print("\n");
    for (;;) hlt();
}

/* ---------- 统一分发（给定） ---------- */
void isr_dispatch(struct regs *r) {
    if (r->int_no < 32) {
        fault_dump(r);
    } else if (r->int_no == 32) {
        ticks++;
        outb(0x20, 0x20);
        schedule();
    } else if (r->int_no == 33) {
        outb(0x20, 0x20);
        kbd_intr();
    } else if (r->int_no == 0x80) {
        syscall_dispatch(r);
    } else {
        if (r->int_no >= 40) outb(0xA0, 0x20);
        outb(0x20, 0x20);
    }
}
