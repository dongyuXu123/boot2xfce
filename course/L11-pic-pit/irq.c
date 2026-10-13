/* L11 桩 —— 8259A PIC 重映射 + PIT 时钟（键盘先给定）
 * 验收：sti 之后 ticks 必须自己走到 3——桩是哑的，永远过不去。 */
#include "kernel.h"

volatile uint32_t ticks = 0;
volatile uint32_t isr_err_code = 0;     /* isr.asm 从异常栈上弹下来的错误码 */

/* ---------- 8259A：把 IRQ 重映射到 0x20-0x2F ---------- */
#define PIC1_CMD 0x20
#define PIC1_DAT 0x21
#define PIC2_CMD 0xA0
#define PIC2_DAT 0xA1

void pic_init(void) {
    /* TODO(你来写·L11): ICW1-ICW4 四步初始化 + 屏蔽字
     * 主从都发 ICW1=0x11 → ICW2 偏移 0x20/0x28 → ICW3 级联 0x04/0x02
     * → ICW4=0x01 → 屏蔽：主片 0xF8(只放 IRQ0/1/2)，从片 0xFF
     * 不重映射会怎样？IRQ0 会撞上 CPU 异常 8 号门——试试就逝世。 */
}

/* ---------- 8253/8254 PIT：通道 0 方波 ---------- */
void pit_init(uint32_t hz) {
    /* TODO(你来写·L11): 除数 = 1193182/hz
     * 0x43 写 0x36（通道0|方波|先低后高），0x40 写除数低/高字节 */
    (void)hz;
}

/* ---------- 键盘（IRQ1）：给定，L18 才轮到你写 ---------- */
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

/* ---------- 统一分发（给定：isr.asm 把所有入口汇到这里） ---------- */
void isr_dispatch(struct regs *r) {
    if (r->int_no < 32) {
        fault_dump(r);                  /* CPU 异常：教学内核直接停机 */
    } else if (r->int_no == 32) {       /* IRQ0 时钟 */
        ticks++;
        outb(0x20, 0x20);               /* 先 EOI 再调度：切换后本栈的 iret 不依赖 PIC */
        schedule();
    } else if (r->int_no == 33) {       /* IRQ1 键盘 */
        outb(0x20, 0x20);
        kbd_intr();
    } else if (r->int_no == 0x80) {
        syscall_dispatch(r);
    } else {
        if (r->int_no >= 40) outb(0xA0, 0x20);
        outb(0x20, 0x20);
    }
}
