/* gdt.c —— 5 个段描述符 + TSS（ring3 系统调用需要 esp0）
 * 平坦模型：内核码/数据 0-4GiB DPL0，用户码/数据 0-4GiB DPL3。
 * 注意：没有分页，ring3 与 ring0 共享同一地址空间（无隔离）——
 * 真正的用户态保护等 S3 开启分页后再说。 */
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
    gdt_set(0, 0, 0, 0x00, 0x00);                  /* null */
    gdt_set(1, 0, 0xFFFFF, 0x9A, 0xCF);            /* 0x08 内核码 */
    gdt_set(2, 0, 0xFFFFF, 0x92, 0xCF);            /* 0x10 内核数据 */
    gdt_set(3, 0, 0xFFFFF, 0xFA, 0xCF);            /* 0x18 用户码 */
    gdt_set(4, 0, 0xFFFFF, 0xF2, 0xCF);            /* 0x20 用户数据 */
    gdt_set(5, (uint32_t)&tss, sizeof(tss) - 1, 0x89, 0x00); /* 0x28 TSS */

    gp.limit = sizeof(gdt) - 1;
    gp.base  = (uint32_t)gdt;
    gdt_flush((uint32_t)&gp);
    tss_flush();
    tss_set_esp0(0);
}
