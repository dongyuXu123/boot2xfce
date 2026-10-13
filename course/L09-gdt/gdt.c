/* L09 桩 —— GDT：5 个段描述符 + TSS
 * 平坦模型：内核码/数据 DPL0，用户码/数据 DPL3，第 5 项 TSS。
 * 验收：sgdt 读出的 limit 必须是 6*8-1=47（kmain 自检）。 */
#include "kernel.h"

struct gdt_entry {
    uint16_t limit_low, base_low;
    uint8_t  base_mid, access, gran, base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

/* 8086 文档顺序的 TSS，i386 起至少 0x68 字节 */
static struct tss_struct {
    uint32_t prev, esp0, ss0, esp1, ss1, ss2, cr3;
    uint32_t eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs, ldt;
    uint16_t trap, iomap;
} tss __attribute__((aligned(8)));

static struct gdt_entry gdt[6];
struct gdt_ptr   gp;      /* cpu.asm 的 gdt_flush 要引用，不能 static */

static void gdt_set(int i, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    /* 给定：把 base/limit/access 拆进描述符（想想为什么 limit 分两处、gran 低位放什么） */
    gdt[i].base_low  = base & 0xFFFF;
    gdt[i].base_mid  = (base >> 16) & 0xFF;
    gdt[i].base_high = (base >> 24) & 0xFF;
    gdt[i].limit_low = limit & 0xFFFF;
    gdt[i].gran      = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[i].access    = access;
}

void tss_set_esp0(uint32_t esp0) {
    tss.esp0 = esp0;
    tss.ss0  = 0x10;                   /* 内核数据段 */
}

void gdt_init(void) {
    /* TODO(你来写·L09): 填 6 个描述符并生效
     * 0: null
     * 1: 0x08 内核代码  base=0 limit=0xFFFFF access=0x9A gran=0xCF
     * 2: 0x10 内核数据  access=0x92
     * 3: 0x18 用户代码  access=0xFA
     * 4: 0x20 用户数据  access=0xF2
     * 5: 0x28 TSS       base=&tss limit=sizeof(tss)-1 access=0x89 gran=0
     * 然后: gp.limit=sizeof(gdt)-1; gp.base=(uint32_t)gdt;
     *       gdt_flush((uint32_t)&gp);  tss_flush();  tss_set_esp0(0);
     * 想想 access 的高 4 位(9/F)和 gran 的 0xCF 各是什么意思，写注释！ */
}
