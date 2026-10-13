/* irq.c —— 8259A PIC 重映射 / PIT 定时器 / 键盘 / 中断 C 侧分发 */
#include "kernel.h"

volatile uint32_t ticks = 0;
volatile uint32_t isr_err_code = 0;     /* isr.asm 从异常栈上弹下来的错误码 */

/* ---------- 8259A：把 IRQ 重映射到 0x20-0x2F，避开 CPU 异常区 ---------- */
#define PIC1_CMD 0x20
#define PIC1_DAT 0x21
#define PIC2_CMD 0xA0
#define PIC2_DAT 0xA1

void pic_init(void) {
    outb(PIC1_CMD, 0x11);               /* ICW1: 级联+ICW4 */
    outb(PIC2_CMD, 0x11);
    outb(PIC1_DAT, 0x20);               /* ICW2: 主片偏移 0x20 */
    outb(PIC2_DAT, 0x28);               /*       从片偏移 0x28 */
    outb(PIC1_DAT, 0x04);               /* ICW3: 主片 IRQ2 接从片 */
    outb(PIC2_DAT, 0x02);
    outb(PIC1_DAT, 0x01);               /* ICW4: 8086 模式 */
    outb(PIC2_DAT, 0x01);
    outb(PIC1_DAT, 0xF8);               /* 屏蔽字：只放行 IRQ0(时钟) IRQ1(键盘) IRQ2(级联) */
    outb(PIC2_DAT, 0xEF);               /* 从片：只放行 IRQ12（鼠标），0xEF = ~0x10 */
}

/* ---------- 8253/8254 PIT：通道 0 方波，驱动调度 ---------- */
void pit_init(uint32_t hz) {
    uint32_t div = 1193182 / hz;
    outb(0x43, 0x36);                   /* 通道0 | 方波 | 先低后高 */
    outb(0x40, div & 0xFF);
    outb(0x40, (div >> 8) & 0xFF);
}

/* ---------- 键盘（IRQ1）：PS/2 set 1 通码 → ASCII 环形缓冲 ---------- */
static uint8_t kbuf[32];
static int kbuf_head, kbuf_tail;

static const char sc2ascii[59] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=','\b','\t',
    'q','w','e','r','t','y','u','i','o','p','[',']','\n', 0,
    'a','s','d','f','g','h','j','k','l',';','\'','`', 0, '\\',
    'z','x','c','v','b','n','m',',','.','/', 0, '*', 0, ' '
};

static void kbd_intr(void) {
    uint8_t sc = inb(0x60);             /* 读通码（同时应答键盘） */
    if (sc & 0x80) return;              /* 释放码忽略 */
    if (sc < sizeof(sc2ascii) && sc2ascii[sc]) {
        int next = (kbuf_head + 1) % 32;
        if (next != kbuf_tail) {        /* 满则丢弃 */
            kbuf[kbuf_head] = sc2ascii[sc];
            kbuf_head = next;
        }
    }
}

int kbd_pop(void) {
    if (kbuf_head == kbuf_tail) return -1;
    int c = kbuf[kbuf_tail];
    kbuf_tail = (kbuf_tail + 1) % 32;
    return c;
}

/* ---------- 异常现场转储（教学用：崩了能看见在哪） ---------- */
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

/* ---------- 统一分发：isr.asm 把所有入口汇到这里 ---------- */
void isr_dispatch(struct regs *r) {
    if (r->int_no == 14) {              /* 缺页：交给分页层（按需/COW），兜不住再炸 */
        page_fault_handler(isr_err_code, read_cr2());
    } else if (r->int_no < 32) {
        fault_dump(r);                  /* CPU 异常：教学内核直接停机 */
    } else if (r->int_no == 32) {       /* IRQ0 时钟 */
        ticks++;
        { static int c = 0; if (c++ < 400 && ticks % 50 == 0) print("[t]"); }
        outb(0x20, 0x20);               /* 先 EOI 再调度：切换后本栈的 iret 不依赖 PIC */
        schedule();
    } else if (r->int_no == 33) {       /* IRQ1 键盘 */
        outb(0x20, 0x20);
        kbd_intr();
        extern void tty_poll(void);
        tty_poll();
    } else if (r->int_no == 44) {       /* IRQ12 鼠标 */
        outb(0xA0, 0x20);
        outb(0x20, 0x20);
        mouse_intr();
    } else if (r->int_no == 0x80) {
        syscall_dispatch(r);
    } else {
        if (r->int_no >= 40) outb(0xA0, 0x20);
        outb(0x20, 0x20);
    }
}
