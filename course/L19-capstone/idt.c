/* idt.c —— 256 个中断门：0-31 异常 / 32-47 IRQ / 0x80 系统调用(DPL=3) */
#include "kernel.h"

struct idt_entry {
    uint16_t base_lo, sel;
    uint8_t  always0, flags;
    uint16_t base_hi;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr   idtp;

/* isr.asm 提供的入口表：0-31 异常，32-47 IRQ，48 = int 0x80 */
extern void *isr_table[];

static void idt_set(int n, uint32_t handler, uint8_t flags) {
    idt[n].base_lo = handler & 0xFFFF;
    idt[n].base_hi = (handler >> 16) & 0xFFFF;
    idt[n].sel     = 0x08;
    idt[n].always0 = 0;
    idt[n].flags   = flags;
}

void idt_init(void) {
    memset(idt, 0, sizeof(idt));
    for (int i = 0; i < 48; i++)
        idt_set(i, (uint32_t)isr_table[i], 0x8E);      /* ring0 门 */
    idt_set(0x80, (uint32_t)isr_table[48], 0xEE);      /* DPL=3：用户可调 */

    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint32_t)idt;
    __asm__ volatile("lidt %0" : : "m"(idtp));
}
