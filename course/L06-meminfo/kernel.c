/* [课程模板 · S1] 被 GRUB 加载的最小内核（进度门控版）
 *
 * 与参考答案的区别：meminfo(L06)/mmap(L07) 两处挖了 TODO 桩，
 * 其余（串口/VGA/打印函数）全部给定——它们是之前课的成果或测试设施。
 * 本文件的 TODO 区域只在 LESSON >= 6/7 时编入。
 * ============================================================ */
#include "progress.h"

typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;
typedef uint32_t           uintptr_t;

#define VGA_MEM ((volatile uint16_t *)0xB8000)
#define COM1    0x3F8

static int vga_row = 1, vga_col = 0;

static inline void outb(uint16_t port, uint8_t v) {
    __asm__ volatile("outb %0,%1" :: "a"(v), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t r;
    __asm__ volatile("inb %1,%0" : "=a"(r) : "Nd"(port));
    return r;
}
void *memset(void *d, int c, unsigned n) {
    uint8_t *p = d;
    while (n--) *p++ = (uint8_t)c;
    return d;
}
void *memcpy(void *d, const void *s, unsigned n) {
    uint8_t *p = d;
    const uint8_t *q = s;
    while (n--) *p++ = *q++;
    return d;
}

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
static void vga_putc(char c) {
    if (c == '\n') { vga_row++; vga_col = 0; }
    else {
        VGA_MEM[vga_row * 80 + vga_col] = (uint16_t)(0x0700 | (uint8_t)c);
        if (++vga_col >= 80) { vga_row++; vga_col = 0; }
    }
    if (vga_row >= 25) { vga_row = 0; }
}
static void print(const char *s) {
    for (; *s; s++) { serial_putc(*s); vga_putc(*s); }
}
static void print_hex(uint32_t v) {
    static const char h[] = "0123456789abcdef";
    char buf[11] = "0x";
    for (int i = 0; i < 8; i++) buf[2 + i] = h[(v >> ((7 - i) * 4)) & 0xF];
    print(buf);
}
static void print_dec(uint32_t v) {
    char b[11];
    int i = 10;
    b[i] = 0;
    do { b[--i] = '0' + v % 10; v /= 10; } while (v);
    print(&b[i]);
}

/* ---------- Multiboot 信息结构 ---------- */
struct mb_info {
    uint32_t flags;
    uint32_t mem_lower, mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count, mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length, mmap_addr;
} __attribute__((packed));
struct mb_mmap {
    uint32_t size;                  /* 不含 size 自身 */
    uint64_t addr, len, type;
} __attribute__((packed));

void kmain(uint32_t magic, struct mb_info *mb) {
    serial_init();
    for (int i = 0; i < 80 * 25; i++) VGA_MEM[i] = 0x0720;

    print("S1: GRUB boot OK, multiboot magic=");
    print_hex(magic);
    print("\n");

#if LESSON >= 6
    if (mb->flags & (1u << 0)) {
        /* TODO(你来写·L06): 打印内存大小
         *   "S1: mem lower=<mem_lower>KB, upper=<mem_upper/1024>MB\n"
         * 提示：print / print_dec 已经给你了，除法用 /1024 换 MB。 */
        print("S1: mem lower=");
        print_dec(mb->mem_lower);
        print("KB, upper=");
        print_dec(mb->mem_upper / 1024);
        print("MB");
        print("\n");
    }
#endif

#if LESSON >= 7
    if (mb->flags & (1u << 6)) {
        /* TODO(你来写·L07): 遍历 E820 内存映射
         *   先打印 "S1: memory map (E820):\n"
         *   每条: "  mmap base=<hex> len=<hex> type=<dec>\n"（最多打 6 条）
         *   最后: "S1: mmap entries=<总数>\n"
         * 大坑提示：记录长度在 e->size（不含自身），步进 = e->size + 4！ */
        struct mb_mmap *e = (struct mb_mmap *)(uintptr_t)mb->mmap_addr;
        struct mb_mmap *end = (struct mb_mmap *)(uintptr_t)(mb->mmap_addr + mb->mmap_length);
        int n = 0;
        for (; (uintptr_t)e < (uintptr_t)end;
             e = (struct mb_mmap *)(uintptr_t)((uintptr_t)e + e->size + 4)) {
            n++;
        }
        print("S1: mmap entries=");
        print_dec((uint32_t)n);
        print("\n");
    }
#endif

    print("S1: stage done, halt\n");
    for (;;) __asm__ volatile("hlt");
}
