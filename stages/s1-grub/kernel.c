/* ============================================================
 * S1 —— 被 GRUB 加载的最小内核（freestanding, 32 位）
 *
 * 没有操作系统可用：自己摸 VGA 文本缓冲(0xB8000)和 COM1(0x3F8)。
 * 从 Multiboot 信息结构里读 GRUB 交给我们的硬件信息：
 *   flags bit0: mem_lower/mem_upper（常规/上位内存）
 *   flags bit6: 内存映射表（E820 风格，S2 物理内存管理的输入）
 *
 * 无 libc：类型自己定义，编译带 -nostdinc，零系统头文件依赖
 * ============================================================ */
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;
typedef uint32_t           uintptr_t;   /* 32 位平坦模型 */

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

/* GCC 即使在 freestanding 下也可能生成对 memset/memcpy 的调用 */
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

/* ---------- 串口 / VGA / 双路输出 ---------- */
static void serial_init(void) {
    outb(COM1 + 1, 0x00);           /* 关串口中断 */
    outb(COM1 + 3, 0x80);           /* DLAB=1 */
    outb(COM1,     0x01);           /* 115200 */
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);           /* 8N1 */
    outb(COM1 + 2, 0xC7);           /* FIFO */
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
    if (vga_row >= 25) { vga_row = 0; }   /* 简化：整屏回卷 */
}
static void print(const char *s) {        /* 串口 + VGA 同步输出 */
    for (; *s; s++) {
        serial_putc(*s);
        vga_putc(*s);
    }
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

/* ---------- Multiboot 信息结构（规范 0.6.96+） ---------- */
struct mb_info {
    uint32_t flags;
    uint32_t mem_lower, mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count, mods_addr;
    uint32_t syms[4];               /* bit4/5/16 三选一，本阶段不用 */
    uint32_t mmap_length, mmap_addr;
} __attribute__((packed));

struct mb_mmap {
    uint32_t size;                  /* 不含 size 自身 */
    uint64_t addr;
    uint64_t len;
    uint32_t type;                  /* 1=可用 2=保留 3=ACPI 4=NVS */
} __attribute__((packed));

void kmain(uint32_t magic, struct mb_info *mb) {
    serial_init();
    for (int i = 0; i < 80 * 25; i++) VGA_MEM[i] = 0x0720; /* 清屏 */

    print("S1: GRUB boot OK, multiboot magic=");
    print_hex(magic);
    print("\n");

    if (mb->flags & (1u << 0)) {
        print("S1: mem lower=");
        print_dec(mb->mem_lower);
        print("KB, upper=");
        print_dec(mb->mem_upper / 1024);
        print("MB\n");
    }
    if (mb->flags & (1u << 6)) {
        struct mb_mmap *e = (struct mb_mmap *)(uintptr_t)mb->mmap_addr;
        struct mb_mmap *end = (struct mb_mmap *)((uintptr_t)mb->mmap_addr + mb->mmap_length);
        int n = 0;
        print("S1: memory map (E820):\n");
        for (; (uintptr_t)e < (uintptr_t)end;
             e = (struct mb_mmap *)((uintptr_t)e + e->size + 4)) {
            if (n < 6) {            /* 串口只打前 6 条，防刷屏 */
                print("  mmap base=");
                print_hex((uint32_t)e->addr);
                print(" len=");
                print_hex((uint32_t)e->len);
                print(" type=");
                print_dec(e->type);
                print("\n");
            }
            n++;
        }
        print("S1: mmap entries=");
        print_dec((uint32_t)n);
        print("\n");
    }
    print("S1: stage1 done, halt\n");
    for (;;) __asm__ volatile("hlt");
}
