/* console.c —— 串口(COM1) + VGA 文本双路输出，及最基础字符串函数 */
#include "kernel.h"

#define VGA_MEM ((volatile uint16_t *)0xB8000)
#define COM1    0x3F8

static int vga_row = 1, vga_col = 0;   /* 第 0 行留给 GRUB 菜单回显 */

void *memset(void *d, int c, uint32_t n) {
    uint8_t *p = d;
    while (n--) *p++ = (uint8_t)c;
    return d;
}
void *memcpy(void *d, const void *s, uint32_t n) {
    uint8_t *p = d;
    const uint8_t *q = s;
    while (n--) *p++ = *q++;
    return d;
}

static void serial_init(void) {
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1,     0x01);              /* 115200 8N1 */
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
    if (vga_row >= 25) { vga_row = 0; vga_col = 0; }
}

void console_init(void) {
    serial_init();
    for (int i = 0; i < 80 * 25; i++) VGA_MEM[i] = 0x0720;
    vga_row = 1; vga_col = 0;
}

void print(const char *s) {
    for (; *s; s++) {
        serial_putc(*s);
        vga_putc(*s);
    }
}

void print_hex(uint32_t v) {
    static const char h[] = "0123456789abcdef";
    char buf[11] = "0x";
    for (int i = 0; i < 8; i++) buf[2 + i] = h[(v >> ((7 - i) * 4)) & 0xF];
    print(buf);
}

void print_dec(uint32_t v) {
    char b[11];
    int i = 10;
    b[i] = 0;
    do { b[--i] = (char)('0' + v % 10); v /= 10; } while (v);
    print(&b[i]);
}
