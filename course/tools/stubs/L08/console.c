/* L08 桩 —— 双路 console：串口 + VGA 文本
 * memset/memcpy 给定（它们是全内核的工具）；你写输出三件套 + 初始化。
 * 验收：kmain 用你的 print 打出 "S2: console OK"（桩是哑的，必然 FAIL）。 */
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
    /* TODO(你来写·L08): 初始化 COM1 115200 8N1 + FIFO
     * 对照 stages/s2-kernel-core/console.c 的答案（或 L02 课你的记忆）*/
}

static void serial_putc(char c) {
    /* TODO(你来写·L08): 轮询 LSR bit5 后把字符写进 COM1 */
    (void)c;
}

static void vga_putc(char c) {
    /* TODO(你来写·L08): 写 0xB8000 文本缓冲
     * '\n' 换行回车；一行 80 列；25 行满则回卷到第 0 行 */
    (void)c;
}

void console_init(void) {
    /* TODO(你来写·L08): serial_init + 清屏(0x0720 空格) + 光标归位(row=1,col=0) */
}

void print(const char *s) {
    /* TODO(你来写·L08): 每个字符同时走 serial_putc 和 vga_putc */
    (void)s;
}

void print_hex(uint32_t v) {
    /* TODO(你来写·L08): 打 0x 前缀 + 8 位小写十六进制（可用局部缓冲+print） */
    (void)v;
}

void print_dec(uint32_t v) {
    /* TODO(你来写·L08): 从个位往回填缓冲，再 print（想想 0 要打 "0"） */
    (void)v;
}
