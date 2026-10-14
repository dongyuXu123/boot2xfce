/* fb.c —— S4：VBE/Dispi 线性帧缓冲（32bpp，直写 LFB）
 * 模式经 0x1CE/0x1CF 设置（数据口 16 位）。LFB 基址：PCI 探测
 * std VGA(bus0/dev2) BAR0，兜底 0xFD000000。LFB 映进 boot_pd
 * （恒等别名），由 gfx 内核任务独占绘制。
 */
#include "kernel.h"
#include "font8x8.h"

#define DISPI_INDEX  0x1CE
#define DISPI_DATA   0x1CF
#define DISPI_ID         0x00
#define DISPI_XRES       0x01
#define DISPI_YRES       0x02
#define DISPI_BPP        0x03
#define DISPI_ENABLE     0x04
#define DISPI_ENABLED    0x01
#define DISPI_LFB        0x40

uint32_t fb_addr, fb_pitch, fb_w, fb_h;

static inline uint16_t inw(uint16_t port) {
    uint16_t r; __asm__ volatile("inw %1,%0" : "=a"(r) : "Nd"(port)); return r;
}
static inline void outw(uint16_t port, uint16_t v) {
    __asm__ volatile("outw %0,%1" :: "a"(v), "Nd"(port));
}
static inline void outd(uint16_t port, uint32_t v) {
    __asm__ volatile("out %0,%1" :: "a"(v), "Nd"(port));
}
static inline uint32_t ind(uint16_t port) {
    uint32_t r; __asm__ volatile("in %1,%0" : "=a"(r) : "Nd"(port)); return r;
}
/* 索引/数据两口都只登记了 16 位访问（QEMU hw/display/vga.c
 * vbe_portio_list_x86：{0,1,2,...}{1,1,2,...}，size=2）。索引用 outb 写
 * 会被 QEMU 静默丢弃，vbe_index 恒为 0（只读 ID），模式切换整体失效——
 * 表现为显存内容正确（pmemsave 可验）但屏幕仍停留文本模式。Linux 的
 * bochs 驱动（drivers/gpu/drm/tiny/bochs.c）同样以 outw 写索引。 */
static void dispi_set(uint16_t idx, uint16_t val) {
    outw(DISPI_INDEX, idx); outw(DISPI_DATA, val);
}
static uint16_t dispi_get(uint16_t idx) {
    outw(DISPI_INDEX, idx); return inw(DISPI_DATA);
}
static uint32_t pci_read(uint32_t bus, uint32_t dev, uint32_t fn, uint32_t reg) {
    outd(0xCF8, 0x80000000u | (bus << 16) | (dev << 11) | (fn << 8) | (reg & 0xFC));
    return ind(0xCFC);
}

void fb_init(void) {
    dispi_set(DISPI_ENABLE, 0);
    dispi_set(DISPI_XRES, 640);
    dispi_set(DISPI_YRES, 480);
    dispi_set(DISPI_BPP, 32);
    dispi_set(DISPI_ENABLE, DISPI_ENABLED | DISPI_LFB);

    /* LFB 基址：PCI 探测 std VGA BAR0（0xFD000000），兜底常量。
     * 分辨率/行距硬编码 640x480x32（与 dispi 设置一致）——
     * 不要从 multiboot fb 字段读：GRUB 文本模式下那描述的是文本平面
     * （0xB8000/720x400），会让所有 fb_pixel 被边界检查拒绝（黑屏）。 */
    uint32_t base = 0xFD000000u;
    uint32_t vdid = pci_read(0, 2, 0, 0);
    if ((vdid & 0xFFFF) == 0x1234) {
        uint32_t bar0 = pci_read(0, 2, 0, 0x10);
        if (bar0) base = bar0 & ~0xFu;
    }
    fb_addr = base; fb_pitch = 640 * 4;
    fb_w = 640; fb_h = 480;
    for (uint32_t va = fb_addr & ~0xFFFFFu; va < fb_addr + 0x400000u; va += 0x1000u)
        page_map(page_kernel_pd(), va, va, 0, 1);
    reload_cr3();

    print("S4: LFB 640x480x32 @");
    print_hex(fb_addr);
    print("\n");
}

void fb_pixel(uint32_t x, uint32_t y, uint32_t rgb) {
    if (x >= fb_w || y >= fb_h) return;
    ((volatile uint32_t *)(uintptr_t)(fb_addr + y * fb_pitch))[x] = rgb;
}
void fb_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t rgb) {
    /* 裁剪一次 + rep stosl 整行填充：TCG 下逐像素 fb_rect 一次全屏
     * 要数百毫秒，重绘频繁时画面长期停在撕裂中间态（clock 每秒重绘
     * 曾把显示管线彻底堵死） */
    if (x >= fb_w || y >= fb_h) return;
    uint32_t x1 = x + w > fb_w ? fb_w : x + w;
    uint32_t y1 = y + h > fb_h ? fb_h : y + h;
    uint32_t n = x1 - x;
    for (uint32_t yy = y; yy < y1; yy++) {
        uint32_t *row = (uint32_t *)(uintptr_t)(fb_addr + yy * fb_pitch) + x;
        __asm__ volatile("cld; rep stosl" : "+c"(n), "+D"(row) : "a"(rgb) : "memory");
        n = x1 - x;
    }
}
void fb_blit(void) { }   /* 直写 LFB；接口为将来双缓冲保留 */

void fb_char(uint32_t x, uint32_t y, char c, uint32_t rgb) {
    const unsigned char *g = font8x8_basic[(int)c & 0x7F];
    for (int j = 0; j < 8; j++)
        for (int i = 0; i < 8; i++)
            if (g[j] & (1u << i)) fb_pixel(x + i, y + j, rgb);
}
void fb_text(uint32_t x, uint32_t y, const char *s, uint32_t rgb) {
    for (; *s; s++, x += 8) fb_char(x, y, *s, rgb);
}
