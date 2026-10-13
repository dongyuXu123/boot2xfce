/* rtc.c —— CMOS 实时钟：时/分/秒（BCD 解码） */
#include "kernel.h"

static uint8_t cmos(uint8_t reg) {
    outb(0x70, reg);
    return inb(0x71);
}
static uint8_t bcd(uint8_t v) { return (uint8_t)((v & 0x0F) + (v >> 4) * 10); }

uint32_t rtc_time(void) {                       /* HHMMSS 十进制打包 */
    while (inb(0x70) == 0x80) { }               /* 更新中简单等待 */
    uint8_t s = bcd(cmos(0x00));
    uint8_t m = bcd(cmos(0x02));
    uint8_t h = bcd(cmos(0x04));
    return (uint32_t)h * 10000u + (uint32_t)m * 100u + s;
}
