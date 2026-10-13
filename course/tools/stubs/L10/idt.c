/* L10 桩 —— IDT：256 个中断门
 * 0-31 CPU 异常（ring0 门 0x8E），32-47 IRQ，0x80 系统调用（DPL3 门 0xEE）。
 * 验收：sidt 读出的 limit 必须是 256*8-1=2047，基址须落在内核镜像区。 */
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
    /* 给定：一个门 = 段内偏移拆两半 + 选择子 + 属性 */
    idt[n].base_lo = handler & 0xFFFF;
    idt[n].base_hi = (handler >> 16) & 0xFFFF;
    idt[n].sel     = 0x08;
    idt[n].always0 = 0;
    idt[n].flags   = flags;
}

void idt_init(void) {
    /* TODO(你来写·L10):
     * 1) memset 清零 idt
     * 2) 0..47 号门：handler = isr_table[i]，flags=0x8E
     * 3) 0x80 号门：handler = isr_table[48]，flags=0xEE（DPL=3，用户可调）
     * 4) 填 idtp（limit/base），lidt 生效 */
}
