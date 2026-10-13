/* L13 桩 —— 内核堆 kmalloc/kfree（PMM 是你 L12 的成果，保留）
 * 验收：p2 释放后，kmalloc(150) 必须精确复用 p2 的地址（分裂逻辑要写对）。 */
#include "kernel.h"

/* ---------- PMM（你的 L12 成果，保留勿动） ---------- */
#define PMM_LIMIT   0x20000000u
#define PMM_FRAMES  (PMM_LIMIT / 4096)
static uint8_t  pmm_bm[PMM_FRAMES / 8];
static uint32_t pmm_total;
static uint32_t pmm_hint;

extern uint8_t kernel_end[];

static inline void bm_set(uint32_t f)  { pmm_bm[f >> 3] |=  (1u << (f & 7)); }
static inline int  bm_get(uint32_t f)  { return pmm_bm[f >> 3] & (1u << (f & 7)); }

static void reserve_range(uint32_t base, uint32_t end) {
    for (uint32_t a = (base + 4095) & ~4095u; a < end; a += 4096)
        bm_set(a / 4096);
}

void pmm_init(struct mb_info *mb) {
    memset(pmm_bm, 0, sizeof(pmm_bm));
    pmm_total = 0;
    if (mb->flags & (1u << 6)) {
        struct mb_mmap *e   = (struct mb_mmap *)(uintptr_t)mb->mmap_addr;
        struct mb_mmap *end = (struct mb_mmap *)(uintptr_t)(mb->mmap_addr + mb->mmap_length);
        for (; (uintptr_t)e < (uintptr_t)end;
             e = (struct mb_mmap *)(uintptr_t)((uintptr_t)e + e->size + 4)) {
            if (e->type != 1) continue;
            uint32_t base = (uint32_t)e->addr;
            uint32_t lim  = (uint32_t)(e->addr + e->len);
            if (lim > PMM_LIMIT) lim = PMM_LIMIT;
            if (base < 0x100000u) base = 0x100000u;
            base = (base + 4095) & ~4095u;
            for (uint32_t a = base; a + 4096 <= lim; a += 4096)
                pmm_total++;
        }
    } else if (mb->flags & 1u) {
        uint32_t lim = 0x100000u + mb->mem_upper * 1024u;
        if (lim > PMM_LIMIT) lim = PMM_LIMIT;
        pmm_total = (lim - 0x100000u) / 4096u;
    }
    reserve_range(0x100000u, (uint32_t)(uintptr_t)kernel_end);
    reserve_range(HEAP_START, HEAP_END);
    reserve_range(USER_BASE, USER_BASE + 0x100000u);
}

uint32_t pmm_total_frames(void) { return pmm_total; }

uint32_t pmm_alloc_frame(void) {
    for (uint32_t f = pmm_hint; f < pmm_total; f++)
        if (!bm_get(f)) { bm_set(f); pmm_hint = f + 1; return f * 4096u; }
    return 0;
}

void pmm_free_frame(uint32_t addr) {
    uint32_t f = addr / 4096;
    if (f < pmm_total) { pmm_bm[f >> 3] &= (uint8_t)~(1u << (f & 7)); pmm_hint = f; }
}

/* ---------- 堆：本课你的作业 ---------- */
struct hb {
    uint32_t    size;      /* 不含块头 */
    uint32_t    used;
    struct hb  *next;
};
static struct hb *heap_head;

void heap_init(void) {
    /* TODO(你来写·L13): 把 [HEAP_START, HEAP_END) 变成一个大空闲块 */
}

void *kmalloc(uint32_t n) {
    /* TODO(你来写·L13): 首次适应 + 够大就分裂
     * 1) n 4 字节对齐
     * 2) 找第一个 !used 且 size>=n 的块
     * 3) 剩余 >= 块头+8 就切出后继空闲块（size/used/next 都要接好）
     * 4) 标 used，返回块头之后的地址 */
    (void)n;
    return NULL;
}

void kfree(void *p) {
    /* TODO(你来写·L13): 块头退回 12 字节标 !used，
     * 然后向后扫描合并相邻空闲块（想想为什么合并后要继续看新邻居） */
    (void)p;
}
